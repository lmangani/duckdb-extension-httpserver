## Discovery API

- [x] POST `/subscribe/{secretHash}` - Register a peer
- [x] GET `/discovery/{secretHash}` - Get list of active peers
- [x] DELETE `/unsubscribe/{secretHash}/{peerId}` - Remove peer

### Start Server
```sql
D SELECT httpserve_enable_discovery('true');
D SELECT httpserve_start('0.0.0.0',9999, '');
┌──────────────────────────────────────┐
│ httpserve_start('0.0.0.0', 9999, '') │
│               varchar                │
├──────────────────────────────────────┤
│ HTTP server started on 0.0.0.0:9999  │
└──────────────────────────────────────┘
```

### Register a Peer under a secret hash
#### CURL
```bash
curl -X POST "https://localhost:9999/subscribe/secretHash" \
  -H "Content-Type: application/json" \
  -d '{ "name": "service1", "endpoint": "http://192.168.1.100:8080", "ttl": 300 }
```
#### SQL
```sql
INSTALL http_client FROM community; LOAD http_client; LOAD json;
WITH __input AS (
    SELECT
      http_post(
          'http://localhost:9999/subscribe/secretHash',
          headers => MAP {
          },
          params => MAP {
            'name': 'quackpipe1',
            'endpoint': 'https://1.1.1.1',
          }
      ) AS res
  ) SELECT res->>'reason' as res, res->>'status' as status FROM __input;
```

### Check `peers` table
```sql
D SELECT name, endpoint, source_address as sourceAddress, peer_id as peerId, metadata, ttl, strftime(registered_at, '%Y-%m-%d %H:%M:%S') as registered_at FROM peers WHERE hash = 'secretHash';
┌──────────┬───────────────────────────┬───────────────┬──────────────────────────────────┬──────────┬───────┬─────────────────────┐
│   name   │         endpoint          │ sourceAddress │              peerId              │ metadata │  ttl  │    registered_at    │
│ varchar  │          varchar          │    varchar    │             varchar              │ varchar  │ int64 │       varchar       │
├──────────┼───────────────────────────┼───────────────┼──────────────────────────────────┼──────────┼───────┼─────────────────────┤
│ service1 │ http://192.168.1.100:8080 │ xxx.xx.xx.xxx │ 0872c98634ce7e608e19aa1a1e6cf784 │ {}       │   300 │ 2024-11-14 19:44:23 │
└──────────┴───────────────────────────┴───────────────┴──────────────────────────────────┴──────────┴───────┴─────────────────────┘
```

### Discover Peers
#### CURL
```bash
curl "http://localhost:9999/discovery/secretHash"
```
#### SQL 
```sql
D SELECT * FROM read_ndjson_auto('http://localhost:9999/discovery/secretHash');
┌──────────┬──────────────────────┬────────────────┬──────────────────────────────┬──────────┬─────────┬─────────────────────┐
│   name   │       endpoint       │ source_address │           peer_id            │ metadata │   ttl   │    registered_at    │
│ varchar  │       varchar        │    varchar     │             uuid             │ varchar  │ varchar │      timestamp      │
├──────────┼──────────────────────┼────────────────┼──────────────────────────────┼──────────┼─────────┼─────────────────────┤
│ service1 │ http://192.168.1.1…  │ 127.0.0.1      │ 0872c986-34ce-7e60-8e19-aa…  │          │ 3600    │ 2024-11-15 14:13:50 │
└──────────┴──────────────────────┴────────────────┴──────────────────────────────┴──────────┴─────────┴─────────────────────┘
D
```

⚠️ minor issue with peer_id being a UUID and clients hating it
