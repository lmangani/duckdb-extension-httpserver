#pragma once

#include "duckdb/main/query_result.hpp"
#include "yyjson.hpp"

using namespace duckdb_yyjson;

namespace duckdb {

class ResultSerializer {
public:
	explicit ResultSerializer(const bool _set_invalid_values_to_null = false)
	    : set_invalid_values_to_null(_set_invalid_values_to_null) {
		doc = yyjson_mut_doc_new(nullptr);
		root = yyjson_mut_arr(doc);
		if (!root) {
			throw SerializationException("Could not create yyjson array");
		}
		yyjson_mut_doc_set_root(doc, root);
	}

	~ResultSerializer() {
		yyjson_mut_doc_free(doc);
	}

	void SerializeChunk(const DataChunk &chunk, vector<string> &names, vector<LogicalType> &types,
	                    bool values_as_array);

	yyjson_mut_val *Serialize(QueryResult &query_result, bool values_as_array);

	yyjson_mut_val *SerializeRowAsArray(const DataChunk &chunk, idx_t row_idx, vector<LogicalType> &types);

	yyjson_mut_val *SerializeRowAsObject(const DataChunk &chunk, idx_t row_idx, vector<string> &names,
	                                     vector<LogicalType> &types);

	static std::string YY_ToString(yyjson_mut_doc *val) {
		auto data = yyjson_mut_write(val, 0, nullptr);
		if (!data) {
			throw SerializationException("Could not render yyjson document");
		}
		std::string json_output(data);
		free(data);
		return json_output;
	}

private:
	void SerializeValue(yyjson_mut_val *parent, const Value &value, optional_ptr<string> name, const LogicalType &type);

	yyjson_mut_doc *doc;
	yyjson_mut_val *root;
	bool set_invalid_values_to_null;
};
} // namespace duckdb
