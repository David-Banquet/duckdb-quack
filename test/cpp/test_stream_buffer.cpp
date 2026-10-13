#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "duckdb.hpp"
#include "duckdb/storage/buffer_manager.hpp"

#include "quack_extension.hpp"
#include "quack_fetch_collector.hpp"
#include "quack_message.hpp"
#include "quack_server.hpp"
#include "quack_session_state.hpp"
#include "quack_settings.hpp"

#include <chrono>
#include <future>

using namespace duckdb;

namespace {

class BufferTestServer : public QuackServer {
public:
	using QuackServer::HandleMessageInternal;
	using QuackServer::QuackServer;
};

SendDataRequestMessage MakeBatch(idx_t index) {
	SendDataRequestMessage request("buffer-test", "input");
	request.SetBatchIndex(index);
	auto chunk = make_uniq<DataChunk>();
	chunk->Initialize(Allocator::DefaultAllocator(), {LogicalType::BIGINT});
	chunk->data[0].Append(Value::BIGINT(42));
	chunk->CheckCardinality(1);
	request.Chunks().push_back(std::move(chunk));
	return request;
}

} // namespace

TEST_CASE("SEND_DATA waits for INSERT buffer capacity and wakes on drain or failure", "[quack][buffer]") {
	DuckDB db;
	db.LoadStaticExtension<QuackExtension>();
	Connection settings(db);
	REQUIRE_FALSE(settings.Query("SET quack_server_stream_buffer_bytes = 1")->HasError());
	// The in-process server fixture uses core crypto for its throwaway token.
	REQUIRE_FALSE(settings.Query("SET force_mbedtls_unsafe = true")->HasError());

	BufferTestServer server(*settings.context, QuackUri("quack:localhost", false), "asdf");
	auto connection = server.GetConnection(server.CreateNewConnection("buffer-test", string(), 60));
	auto session = QuackSessionState::Get(*connection->duckdb_connection->context);
	auto stream =
	    session->Streams().Create("input", {LogicalType::BIGINT}, true, QuackServerStreamBufferBytes(*db.instance));

	auto first = MakeBatch(1);
	REQUIRE(first.Chunks()[0]->GetAllocationSize() > 1);
	REQUIRE(server.HandleMessageInternal(*db.instance, first, connection.get())->Type() ==
	        MessageType::SEND_DATA_RESPONSE);

	std::promise<void> started;
	auto second = std::async(std::launch::async, [&]() {
		auto request = MakeBatch(2);
		started.set_value();
		return server.HandleMessageInternal(*db.instance, request, connection.get());
	});
	started.get_future().wait();
	auto blocked = second.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout;

	QuackClaimPopStatus pop_status = QuackClaimPopStatus::EMPTY;
	MessageType expected_type = MessageType::SEND_DATA_RESPONSE;
	SECTION("draining the first batch admits the waiting send") {
		QuackChunkBatch chunks;
		pop_status = stream->buffer.TryPopClaimed(1, chunks);
	}
	SECTION("a failed stream releases the waiting send with the error") {
		stream->buffer.SetError(ErrorData(ExceptionType::IO, "insert failed"));
		expected_type = MessageType::ERROR_RESPONSE;
	}

	// Always release the worker before asserting, including when admission or wakeup is broken.
	auto resumed = second.wait_for(std::chrono::seconds(5));
	if (resumed != std::future_status::ready) {
		stream->buffer.SetError(ErrorData(ExceptionType::IO, "test cleanup"));
	}
	auto response = second.get();
	REQUIRE(blocked);
	REQUIRE(resumed == std::future_status::ready);
	REQUIRE(response->Type() == expected_type);
	if (expected_type == MessageType::SEND_DATA_RESPONSE) {
		REQUIRE(pop_status == QuackClaimPopStatus::BATCH);
	} else {
		REQUIRE(response->Cast<ErrorResponse>().Error().RawMessage() == "insert failed");
	}
}

TEST_CASE("FETCH capacity is active immediately after construction", "[quack][buffer]") {
	QuackResultStream stream(10);
	QuackFetchPayload first;
	QuackFetchPayload second;
	REQUIRE(stream.buffer.TryPushBatch(1, first, 10, nullptr) == QuackPushStatus::PUSHED);
	REQUIRE(stream.buffer.TryPushBatch(2, second, 1, nullptr) == QuackPushStatus::NO_CAPACITY);
	REQUIRE(stream.buffer.TryPopClaimed(1, first) == QuackClaimPopStatus::BATCH);
	REQUIRE(stream.buffer.TryPushBatch(2, second, 1, nullptr) == QuackPushStatus::PUSHED);
}

TEST_CASE("A full stream admits an earlier batch and zero capacity disables the limit", "[quack][buffer]") {
	QuackClaimBuffer<idx_t> bounded(1);
	idx_t batch = 2;
	REQUIRE(bounded.TryPushBatch(2, batch, 2, nullptr) == QuackPushStatus::PUSHED);
	REQUIRE(bounded.TryPushBatch(3, batch, 2, nullptr) == QuackPushStatus::NO_CAPACITY);
	REQUIRE(bounded.TryPushBatch(1, batch, 2, nullptr) == QuackPushStatus::PUSHED);

	QuackClaimBuffer<idx_t> unbounded(0);
	REQUIRE(unbounded.TryPushBatch(1, batch, 2, nullptr) == QuackPushStatus::PUSHED);
	REQUIRE(unbounded.TryPushBatch(2, batch, 2, nullptr) == QuackPushStatus::PUSHED);
}

TEST_CASE("The server stream budget preserves the memory clamp and zero override", "[quack][buffer]") {
	DuckDB db;
	db.LoadStaticExtension<QuackExtension>();
	Connection settings(db);
	REQUIRE_FALSE(settings.Query("SET memory_limit = '4MB'")->HasError());
	auto memory_cap = BufferManager::GetBufferManager(*db.instance).GetOperatorMemoryLimit() / 4;
	REQUIRE(memory_cap > 0);
	REQUIRE(memory_cap < QUACK_SERVER_STREAM_BUFFER_BYTES_DEFAULT);
	REQUIRE(QuackServerStreamBufferBytes(*db.instance) == memory_cap);

	REQUIRE_FALSE(settings.Query("SET quack_server_stream_buffer_bytes = 1")->HasError());
	REQUIRE(QuackServerStreamBufferBytes(*db.instance) == 1);
	REQUIRE_FALSE(settings.Query("SET quack_server_stream_buffer_bytes = 0")->HasError());
	REQUIRE(QuackServerStreamBufferBytes(*db.instance) == 0);
}
