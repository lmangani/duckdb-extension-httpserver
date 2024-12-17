#include "httpserver_extension/http_handler/common.hpp"
#include "duckdb.hpp"

#include "yyjson.hpp"
#include <string>

using namespace duckdb;
using namespace duckdb_yyjson;

namespace duckdb_httpserver {

static std::string GetColumnType(MaterializedQueryResult& result, idx_t column) {
	if (result.RowCount() == 0) {
		return "String";
	}
	switch (result.types[column].id()) {
		case LogicalTypeId::FLOAT:
			return "Float";
		case LogicalTypeId::DOUBLE:
			return "Double";
		case LogicalTypeId::INTEGER:
			return "Int32";
		case LogicalTypeId::BIGINT:
			return "Int64";
		case LogicalTypeId::UINTEGER:
			return "UInt32";
		case LogicalTypeId::UBIGINT:
			return "UInt64";
		case LogicalTypeId::VARCHAR:
			return "String";
		case LogicalTypeId::TIME:
			return "DateTime";
		case LogicalTypeId::DATE:
			return "Date";
		case LogicalTypeId::TIMESTAMP:
			return "DateTime";
		case LogicalTypeId::BOOLEAN:
			return "Int8";
		default:
			return "String";
	}
	return "String";
}

// Convert the query result to JSON format
std::string ConvertResultToJSON(MaterializedQueryResult& result, QueryExecStats& stats) {
    auto doc = yyjson_mut_doc_new(nullptr);
    auto root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    // Add meta information
    auto meta_array = yyjson_mut_arr(doc);
    for (idx_t col = 0; col < result.ColumnCount(); ++col) {
        auto column_obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, column_obj, "name", result.ColumnName(col).c_str());
        yyjson_mut_arr_append(meta_array, column_obj);
        std::string tp(GetColumnType(result, col));
        yyjson_mut_obj_add_strcpy(doc, column_obj, "type", tp.c_str());
    }
    yyjson_mut_obj_add_val(doc, root, "meta", meta_array);

    // Add data
    auto data_array = yyjson_mut_arr(doc);
    for (idx_t row = 0; row < result.RowCount(); ++row) {
        auto row_array = yyjson_mut_arr(doc);
        for (idx_t col = 0; col < result.ColumnCount(); ++col) {
            Value value = result.GetValue(col, row);
            if (value.IsNull()) {
                yyjson_mut_arr_append(row_array, yyjson_mut_null(doc));
            } else {
                std::string value_str = value.ToString();
                yyjson_mut_arr_append(row_array, yyjson_mut_strncpy(doc, value_str.c_str(), value_str.length()));
            }
        }
        yyjson_mut_arr_append(data_array, row_array);
    }
    yyjson_mut_obj_add_val(doc, root, "data", data_array);

    // Add row count
    yyjson_mut_obj_add_int(doc, root, "rows", result.RowCount());

    //"statistics":{"elapsed":0.00031403,"rows_read":1,"bytes_read":0}}
    auto stat_obj = yyjson_mut_obj_add_obj(doc, root, "statistics");
    yyjson_mut_obj_add_real(doc, stat_obj, "elapsed", stats.elapsed_sec);
    yyjson_mut_obj_add_int(doc, stat_obj, "rows_read", stats.read_rows);
    yyjson_mut_obj_add_int(doc, stat_obj, "bytes_read", stats.read_bytes);

    // Write to string
    auto data = yyjson_mut_write(doc, 0, nullptr);
    if (!data) {
        yyjson_mut_doc_free(doc);
        throw InternalException("Failed to render the result as JSON, yyjson failed");
    }

    std::string json_output(data);
    free(data);
    yyjson_mut_doc_free(doc);
    return json_output;
}

// Convert the query result to NDJSON (JSONEachRow) format
std::string ConvertResultToNDJSON(MaterializedQueryResult& result) {
    std::string ndjson_output;

    for (idx_t row = 0; row < result.RowCount(); ++row) {
        // Create a new JSON document for each row
        auto doc = yyjson_mut_doc_new(nullptr);
        auto root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);

        for (idx_t col = 0; col < result.ColumnCount(); ++col) {
            Value value = result.GetValue(col, row);
            const char* column_name = result.ColumnName(col).c_str();

            // Handle null values and add them to the JSON object
            if (value.IsNull()) {
                yyjson_mut_obj_add_null(doc, root, column_name);
            } else {
                // Convert value to string and add it to the JSON object
                std::string value_str = value.ToString();
                yyjson_mut_obj_add_strncpy(doc, root, column_name, value_str.c_str(), value_str.length());
            }
        }

        char *json_line = yyjson_mut_write(doc, 0, nullptr);
        if (!json_line) {
            yyjson_mut_doc_free(doc);
            throw InternalException("Failed to render a row as JSON, yyjson failed");
        }

        ndjson_output += json_line;
        ndjson_output += "\n";

        // Free allocated memory for this row
        free(json_line);
        yyjson_mut_doc_free(doc);
    }

    return ndjson_output;
}

} // namespace duckdb_httpserver
