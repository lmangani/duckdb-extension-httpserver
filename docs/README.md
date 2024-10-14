<img src="https://github.com/user-attachments/assets/35bfded5-3f21-46b5-91f7-014f5a09fac3" width=200 />

# DuckDB HTTP Server Extension
This very experimental extension spawns an HTTP Server from within DuckDB serving query requests.<br>
The extension goal is to replace the functionality currently offered by our project [Quackpipe](https://github.com/metrico/quackpipe)

#### Screenshot
<img src="https://github.com/user-attachments/assets/b79aa620-c81e-4469-8d56-cbef3b1a60e4" width=800 />


### Extension Functions
- `httpserve_start(host, port)`
- `httpserve_stop()`

### API Endpoints
- `/query` 
- `/health`

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
curl -X POST -d "LOAD chsql; SELECT *, uuid() FROM numbers(10)" http://localhost:9999/
```
```json
{"meta":[{"name":"number"},{"name":"uuid()"}],"data":[["0","6a450270-3588-42ff-81db-4b1fc8f00cf5"],["1","de8cc0a2-b1ca-4f46-9ad1-07b21903d8fd"],["2","1cae7b3b-0186-486f-9db3-9da84d2752b0"],["3","97b02542-6645-4128-b4bc-97dba8030b7c"],["4","58699c81-9bcd-435e-8e2a-6def5700517b"],["5","d7b14f00-7b61-41ee-987a-2c5326ff2019"],["6","4f355788-482e-4399-b3e5-cf1e628b2c8b"],["7","58b18db2-14c5-416f-80b8-4d04ec4e31f5"],["8","c459de79-027e-468f-93c4-a896ca885a37"],["9","f591ce48-56de-4738-a5ea-c5c6c9f46479"]],"rows":10}
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
