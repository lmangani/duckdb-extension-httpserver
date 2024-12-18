#pragma once

#include "duckdb/common/extra_type_info.hpp"
#include "duckdb/common/types/uuid.hpp"
#include "duckdb/main/query_result.hpp"
#include "yyjson.hpp"

#include <iostream>

using namespace duckdb_yyjson;

namespace duckdb {

class SerializationResult {
public:
	virtual ~SerializationResult() = default;
	virtual bool IsSuccess() = 0;
	virtual string WithSuccessField() = 0;
	virtual string Raw() = 0;

	void Print() {
		std::cerr << WithSuccessField() << std::endl;
	}

	template <class TARGET>
	TARGET &Cast() {
		DynamicCastCheck<TARGET>(this);
		return reinterpret_cast<TARGET &>(*this);
	}
};

class SerializationSuccess final : public SerializationResult {
public:
	explicit SerializationSuccess(string serialized) : serialized(std::move(serialized)) {
	}

	bool IsSuccess() override {
		return true;
	}

	string Raw() override {
		return serialized;
	}

	string WithSuccessField() override {
		return R"({"success": true, "data": )" + serialized + "}";
	}

private:
	string serialized;
};

class SerializationError final : public SerializationResult {
public:
	explicit SerializationError(string message) : message(std::move(message)) {
	}

	bool IsSuccess() override {
		return false;
	}

	string Raw() override {
		return message;
	}

	string WithSuccessField() override {
		return R"({"success": false, "message": ")" + message + "\"}";
	}

private:
	string message;
};

class ResultSerializer {
public:
	explicit ResultSerializer(const bool _set_invalid_values_to_null = false)
	    : set_invalid_values_to_null(_set_invalid_values_to_null) {
		doc = yyjson_mut_doc_new(nullptr);
		root = yyjson_mut_arr(doc);
		if (!root) {
			throw InternalException("Could not create yyjson array");
		}
		yyjson_mut_doc_set_root(doc, root);
	}
	unique_ptr<SerializationResult> result;

	~ResultSerializer() {
		yyjson_mut_doc_free(doc);
	}

	void SerializeChunk(const DataChunk &chunk, vector<string> &names, vector<LogicalType> &types);

	yyjson_mut_val* Serialize(QueryResult &query_result);

private:


	void SerializeValue(
	    yyjson_mut_val *parent, const Value &value, optional_ptr<string> name, const LogicalType &type
	);

	yyjson_mut_doc *doc;
	yyjson_mut_val *root;
	bool set_invalid_values_to_null;
};
} // namespace duckdb
