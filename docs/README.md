<!-- <img src="https://github.com/user-attachments/assets/35bfded5-3f21-46b5-91f7-014f5a09fac3" width=200 /> -->

<img src="https://github.com/user-attachments/assets/46a5c546-7e9b-42c7-87f4-bc8defe674e0" width=250 />


# DuckDB HTTP Server Extension
This very experimental extension spawns an HTTP Server from within DuckDB serving query requests.<br>
The extension goal is to replace the functionality currently offered by [Quackpipe](https://github.com/metrico/quackpipe)

![image](https://github.com/user-attachments/assets/e930a8d2-b3e4-454e-ba12-e5e91b30bfbe)

#### Extension Functions
- `httpserve_start(host, port)`
- `httpserve_stop()`

#### API Endpoints
- `/` `GET`, `POST`
  - `default_format`: Supports `JSONEachRow` or `JSONCompact`
  - `query`: Supports DuckDB SQL queries
- `/ping` `GET`

### Installation
```sql
INSTALL httpserver FROM community;
LOAD httpserver;
```

### Usage
Start the HTTP server providing the `host` and `port` parameters
```sql
D SELECT httpserve_start('0.0.0.0',9999);
┌─────────────────────────────────────┐
│  httpserve_start('0.0.0.0', 9999)   │
│               varchar               │
├─────────────────────────────────────┤
│ HTTP server started on 0.0.0.0:9999 │
└─────────────────────────────────────┘
```

#### QUERY UI
Browse to your endpoint and use the built-in quackplay interface _(experimental)_
![image](https://github.com/user-attachments/assets/0ee751d0-7360-4d3d-949d-3fb930634ebd)

#### QUERY API
Query your API endpoint using curl GET/POST requests

```bash
curl -X POST -d "SELECT 'hello', version()" "http://localhost:9999/?default_format=JSONCompact
```
```json
{
  "meta": [
    {
      "name": "'hello'",
      "type": "String"
    },
    {
      "name": "\"version\"()",
      "type": "String"
    }
  ],
  "data": [
    [
      "hello",
      "v1.1.1"
    ]
  ],
  "rows": 1,
  "statistics": {
    "elapsed": 0.01,
    "rows_read": 1,
    "bytes_read": 0
  }
}
```

You can also have DuckDB instances query each other using `NDJSON`

```sql
D LOAD json;
D LOAD httpfs;
D SELECT httpserve_start('0.0.0.0', 9999);
┌─────────────────────────────────────┐
│  httpserve_start('0.0.0.0', 9999)   │
│               varchar               │
├─────────────────────────────────────┤
│ HTTP server started on 0.0.0.0:9999 │
└─────────────────────────────────────┘
D SELECT * FROM read_json_auto('http://localhost:9999/?q=SELECT version()');
┌─────────────┐
│ "version"() │
│   varchar   │
├─────────────┤
│ v1.1.1      │
└─────────────┘
```

<br>

<br>

### Build steps
Now to build the extension, run:
```sh
make
```
The main binaries that will be built are:
```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/<extension_name>/<extension_name>.duckdb_extension
```
- `duckdb` is the binary for the duckdb shell with the extension code automatically loaded. 
- `unittest` is the test runner of duckdb. Again, the extension is already linked into the binary.
- `<extension_name>.duckdb_extension` is the loadable binary as it would be distributed.

## Running the extension
To run the extension code, simply start the shell with `./build/release/duckdb`. This shell will have the extension pre-loaded.  

## Running the tests
Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:
```sh
make test
```
