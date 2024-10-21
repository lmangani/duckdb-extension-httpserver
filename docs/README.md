<!-- <img src="https://github.com/user-attachments/assets/35bfded5-3f21-46b5-91f7-014f5a09fac3" width=200 /> -->

<img src="https://github.com/user-attachments/assets/46a5c546-7e9b-42c7-87f4-bc8defe674e0" width=250 />


# DuckDB HTTP Server Extension
This very experimental extension spawns an HTTP Server from within DuckDB serving query requests.<br>
The extension goal is to replace the functionality currently offered by [quackpipe](https://github.com/metrico/quackpipe)

### Features

- Turn any [DuckDB](https://duckdb.org) instance into an **HTTP OLAP API** Server
- Use the embedded **Play User Interface** to query and visualize data 
- Pair with [chsql](https://community-extensions.duckdb.org/extensions/chsql.html) extension for **ClickHouse flavoured SQL**
- Work with local and remote datasets including [MotherDuck](https://motherduck.com) 🐤
- _100% Opensource, ready to use and extend by the Community!_

<br>

![image](https://github.com/user-attachments/assets/e930a8d2-b3e4-454e-ba12-e5e91b30bfbe)

#### Extension Functions
- `httpserve_start(host, port, auth)`: starts the server using provided parameters
- `httpserve_stop()`: stops the server thread

#### Notes

🛑 Run DuckDB in `-readonly` mode for enhanced security

<br>

### 📦 [Installation](https://community-extensions.duckdb.org/extensions/httpserver.html)
```sql
INSTALL httpserver FROM community;
LOAD httpserver;
```

### 🔌 Usage
Start the HTTP server providing the `host`, `port` and `auth` parameters.<br>
> * If you want no authentication, just pass an empty string as parameter.<br>
> * If you want the API run in foreground set `DUCKDB_HTTPSERVER_FOREGROUND=1`

#### Basic Auth
```sql
D SELECT httpserve_start('localhost', 9999, 'user:pass');

┌───────────────────────────────────────────────┐
│ httpserve_start('0.0.0.0', 9999, 'user:pass') │
│                    varchar                    │
├───────────────────────────────────────────────┤
│ HTTP server started on 0.0.0.0:9999           │
└───────────────────────────────────────────────┘
```
```bash
curl -X POST -d "SELECT 'hello', version()" "http://user:pass@localhost:9999/"
```

#### Token Auth
```sql
SELECT httpserve_start('localhost', 9999, 'supersecretkey');

┌───────────────────────────────────────────────┐
│ httpserve_start('0.0.0.0', 9999, 'secretkey') │
│                    varchar                    │
├───────────────────────────────────────────────┤
│ HTTP server started on 0.0.0.0:9999           │
└───────────────────────────────────────────────┘
```

Query your endpoint using the `X-API-Key` token:

```bash
curl -X POST --header "X-API-Key: secretkey" -d "SELECT 'hello', version()" "http://localhost:9999/"
```

You can perform the same action from DuckDB using HTTP `extra_http_headers`:

```sql
D CREATE SECRET extra_http_headers (
      TYPE HTTP,
      EXTRA_HTTP_HEADERS MAP{
          'X-API-Key': 'secret'
      }
  );

D SELECT * FROM duck_flock('SELECT version()', ['http://localhost:9999']);
┌─────────────┐
│ "version"() │
│   varchar   │
├─────────────┤
│ v1.1.1      │
└─────────────┘
```



#### 👉 QUERY UI
Browse to your endpoint and use the built-in quackplay interface _(experimental)_

![image](https://github.com/user-attachments/assets/0ee751d0-7360-4d3d-949d-3fb930634ebd)

#### 👉 QUERY API
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

#### 👉 CROSS-OVER EXAMPLES

You can now have DuckDB instances query each other and... _themselves!_

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

#### Flock Macro by @carlopi
Check out this flocking macro from fellow _Italo-Amsterdammer_ @carlopi @ DuckDB Labs

![image](https://github.com/user-attachments/assets/b409ec0e-86e0-4a8d-822c-377ddbae524d)

* a DuckDB CLI, running httpserver extension
* a DuckDB from Python, running httpserver extension
* a DuckDB from the Web, querying all 3 DuckDB at the same time

<br>

<hr>

<br>

### API Documentation

#### Endpoints Overview

| Endpoint | Methods | Description |
|----------|---------|-------------|
| `/`      | GET, POST | Query API endpoint |
| `/ping`  | GET       | Health check endpoint |

#### Detailed Endpoint Specifications

##### Query API

**Methods:** `GET`, `POST`

**Parameters:**

| Parameter | Description | Supported Values |
|-----------|-------------|-------------------|
| `default_format` | Specifies the output format | `JSONEachRow`, `JSONCompact` |
| `query` | The DuckDB SQL query to execute | Any valid DuckDB SQL query |

##### Notes

- Ensure that your queries are properly formatted and escaped when sending them as part of the request.
- The root endpoint (`/`) supports both GET and POST methods, but POST is recommended for complex queries or when the query length exceeds URL length limitations.
- Always specify the `default_format` parameter to ensure consistent output formatting.

<br>

##### :black_joker: Disclaimers 

[^1]: DuckDB ® is a trademark of DuckDB Foundation. All rights reserved by their respective owners. [^1]
[^2]: ClickHouse ® is a trademark of ClickHouse Inc. No direct affiliation or endorsement. [^2]
[^3]: Released under the MIT license. See LICENSE for details. All rights reserved by their respective owners. [^3]
