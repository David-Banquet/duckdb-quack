//===----------------------------------------------------------------------===//
//                         DuckDB
//
// quack_settings.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/typedefs.hpp"

namespace duckdb {

class ClientContext;
class DatabaseInstance;

//! The defaults for the tuning settings. quack_extension.cpp and every read site use these.
static constexpr idx_t QUACK_TARGET_BATCH_BYTES_DEFAULT = 32ULL * 1024ULL * 1024ULL;
static constexpr idx_t QUACK_REBALANCE_BUFFER_BYTES_DEFAULT = 0;
static constexpr idx_t QUACK_FETCH_PRODUCER_BUFFER_BYTES_DEFAULT = 256ULL * 1024ULL * 1024ULL;
static constexpr idx_t QUACK_PREPARE_INLINE_ROWS_DEFAULT = 24576;

idx_t QuackGetUBigintSetting(ClientContext &context, const char *name, idx_t default_value);
idx_t QuackGetUBigintSetting(DatabaseInstance &db, const char *name, idx_t default_value);

} // namespace duckdb
