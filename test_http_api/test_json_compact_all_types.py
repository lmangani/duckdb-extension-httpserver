from .client import Client, ResponseFormat
from .responses.all_types_compact import ALL_TYPES_COMPACT


def test_json_compact_all_types(http_duck_with_token: Client):
    res = http_duck_with_token.execute_query("FROM test_all_types()", response_format=ResponseFormat.COMPACT_JSON)

    assert res["meta"] == ALL_TYPES_COMPACT["meta"]
    assert res["data"] == ALL_TYPES_COMPACT["data"]
    assert res["rows"] == ALL_TYPES_COMPACT["rows"]
