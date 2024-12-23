import subprocess
from typing import Iterator

import pytest

from .client import Client
from .const import DEBUG_SHELL, HOST, PORT, API_KEY


@pytest.fixture
def http_duck_with_token() -> Iterator[Client]:
    process = subprocess.Popen(
        [
            DEBUG_SHELL,
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=2^16
    )

    # Load the extension
    process.stdin.write("LOAD httpserver;\n")
    cmd = f"SELECT httpserve_start('{HOST}', {PORT}, '{API_KEY}');\n"
    process.stdin.write(cmd)

    client = Client(f"http://{HOST}:{PORT}", token_auth=API_KEY)
    client.on_ready()
    yield client

    process.kill()
