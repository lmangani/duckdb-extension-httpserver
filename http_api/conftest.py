import subprocess

import pytest

from .client import Client
from .const import DEBUG_SHELL, HOST, PORT, API_KEY



@pytest.fixture
def http_duck_with_token():
    process = subprocess.Popen(
        [
            DEBUG_SHELL,
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    # Load the extension
    process.stdin.write("LOAD httpserver;\n")
    cmd = f"SELECT httpserve_start('{HOST}', {PORT}, '{API_KEY}');\n"
    process.stdin.write(cmd)
    yield

    process.kill()


@pytest.fixture
def token_client():
    return Client(f"http://{HOST}:{PORT}", token_auth=API_KEY)
