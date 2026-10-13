#include "quack_settings.hpp"

#include "duckdb/main/client_context.hpp"
#include "duckdb/main/config.hpp"

namespace duckdb {

idx_t QuackGetUBigintSetting(ClientContext &context, const char *name, idx_t default_value) {
	Value val;
	if (context.TryGetCurrentSetting(name, val) && !val.IsNull()) {
		return val.GetValue<uint64_t>();
	}
	return default_value;
}

idx_t QuackGetUBigintSetting(DatabaseInstance &db, const char *name, idx_t default_value) {
	Value val;
	if (DBConfig::GetConfig(db).TryGetCurrentSetting(name, val) && !val.IsNull()) {
		return val.GetValue<uint64_t>();
	}
	return default_value;
}

} // namespace duckdb
