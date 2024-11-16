#ifndef DUCK_FLOCK_H
#define DUCK_FLOCK_H
#include "httpserver_extension.hpp"
namespace duckdb {
    struct DuckFlockData : FunctionData{
        vector<unique_ptr<Connection>> conn;
        vector<unique_ptr<QueryResult>> results;
        unique_ptr<FunctionData> Copy() const override {
            throw std::runtime_error("not implemented");
        }
        bool Equals(const FunctionData &other) const override {
            throw std::runtime_error("not implemented");
        };
    };



    unique_ptr<FunctionData> DuckFlockBind(ClientContext &context, TableFunctionBindInput &input,
                                                        vector<LogicalType> &return_types, vector<string> &names) {
        auto data = make_uniq<DuckFlockData>();
        auto strQuery = input.inputs[0].GetValue<string>();
        vector<string> flock;
        auto &raw_flock = ListValue::GetChildren(input.inputs[1]);
        for (auto &duck : raw_flock) {
            flock.push_back(duck.ToString());
            auto conn = make_uniq<Connection>(*context.db);
            conn->Query("INSTALL json;LOAD json;INSTALL httpfs;LOAD httpfs;");
            auto req = conn->Prepare("SELECT * FROM read_json($2 || '/?q=' || url_encode($1::VARCHAR))");
            if (req->HasError()) {
                throw std::runtime_error("duck_flock: error: " + req->GetError());
            }
            data->conn.push_back(std::move(conn));
            data->results.push_back(std::move(req->Execute(strQuery.c_str(), duck.ToString())));
        }
        if (data->results[0]->HasError()) {
            throw std::runtime_error("duck_flock: error: " + data->results[0]->GetError());
        }
        return_types.clear();
        copy(data->results[0]->types.begin(), data->results[0]->types.end(), back_inserter(return_types));
        names.clear();
        copy(data->results[0]->names.begin(), data->results[0]->names.end(), back_inserter(names));
        return std::move(data);
    }

    void DuckFlockImplementation(ClientContext &context, duckdb::TableFunctionInput &data_p,
                                      DataChunk &output) {
        auto &data = data_p.bind_data->Cast<DuckFlockData>();
        for (const auto &res : data.results) {
            ErrorData error_data;
            unique_ptr<DataChunk> data_chunk = make_uniq<DataChunk>();
            if (res->TryFetch(data_chunk, error_data)) {
                if (data_chunk != nullptr) {
                    output.Append(*data_chunk);
                    return;
                }
            }
        }
    }

    TableFunction DuckFlockTableFunction() {
      TableFunction f(
          "url_flock",
          {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},
          DuckFlockImplementation,
          DuckFlockBind,
          nullptr,
          nullptr
      );
      return f;
    }


}




#endif
