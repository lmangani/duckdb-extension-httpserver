# DuckDB HTTP Server Extension
This very experimental extension spawns an HTTP Server from within DuckDB serving query requests.

<img src="https://github.com/user-attachments/assets/b79aa620-c81e-4469-8d56-cbef3b1a60e4" width=800 />


### Extension Functions
- `httpserve_start(host, port)`
- `httpserve_stop()`

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

#### Query
Query the endpoint using curl GET/POST requests
```bash
# curl -X POST -d "SELECT 42 as answer" http://localhost:9999/query
answer
INTEGER
[ Rows: 1]
42
```

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

```
D SELECT httpserve_start('0.0.0.0',9999);
┌─────────────────────────────────────┐
│  httpserve_start('0.0.0.0', 9999)   │
│               varchar               │
├─────────────────────────────────────┤
│ HTTP server started on 0.0.0.0:9999 │
└─────────────────────────────────────┘
```

## Running the tests
Different tests can be created for DuckDB extensions. The primary way of testing DuckDB extensions should be the SQL tests in `./test/sql`. These SQL tests can be run using:
```sh
make test
```
