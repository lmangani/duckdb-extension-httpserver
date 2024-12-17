#include "httpserver_extension/http_handler/common.hpp"
#include "duckdb.hpp"
#include "yyjson.hpp"
#include <string>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

using namespace duckdb;
using namespace duckdb_yyjson;

namespace duckdb_httpserver {

static BoundParameterData ExtractQueryParameter(const std::string& key, yyjson_val* parameterVal) {
    if (!yyjson_is_obj(parameterVal)) {
    	throw HttpHandlerException(400, "The parameter `" + key + "` must be an object");
    }

    auto typeVal = yyjson_obj_get(parameterVal, "type");
    if (!typeVal) {
    	throw HttpHandlerException(400, "The parameter `" + key + "` does not have a `type` field");
    }
	if (!yyjson_is_str(typeVal)) {
		throw HttpHandlerException(400, "The field `type` for the parameter `" + key + "` must be a string");
	}
	auto type = std::string(yyjson_get_str(typeVal));

	auto valueVal = yyjson_obj_get(parameterVal, "value");
    if (!valueVal) {
    	throw HttpHandlerException(400, "The parameter `" + key + "` does not have a `value` field");
    }

    if (type == "TEXT") {
    	if (!yyjson_is_str(valueVal)) {
    		throw HttpHandlerException(400, "The field `value` for the parameter `" + key + "` must be a string");
    	}

    	return BoundParameterData(Value(yyjson_get_str(valueVal)));
    }
	else if (type == "BOOLEAN") {
    	if (!yyjson_is_bool(valueVal)) {
    		throw HttpHandlerException(400, "The field `value` for the parameter `" + key + "` must be a boolean");
    	}

    	return BoundParameterData(Value(bool(yyjson_get_bool(valueVal))));
    }

    throw HttpHandlerException(400, "Unsupported type " + type + " the parameter `" + key + "`");
}

case_insensitive_map_t<BoundParameterData> ExtractQueryParameters(yyjson_val* parametersVal) {
    if (!parametersVal || !yyjson_is_obj(parametersVal)) {
    	throw HttpHandlerException(400, "The `parameters` field must be an object");
    }

 	case_insensitive_map_t<BoundParameterData> named_values;

	size_t idx, max;
	yyjson_val *parameterKeyVal, *parameterVal;
	yyjson_obj_foreach(parametersVal, idx, max, parameterKeyVal, parameterVal) {
		auto parameterKeyString = std::string(yyjson_get_str(parameterKeyVal));

		named_values[parameterKeyString] = ExtractQueryParameter(parameterKeyString, parameterVal);
	}

 	return named_values;
}

} // namespace duckdb_httpserver
