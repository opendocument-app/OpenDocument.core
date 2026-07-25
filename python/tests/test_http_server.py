import socket
import threading
import time
import urllib.request

import pytest

import pyodr


def free_port():
    with socket.socket() as sock:
        sock.bind(("localhost", 0))
        return sock.getsockname()[1]


def fetch(url, timeout=5.0):
    deadline = time.monotonic() + timeout
    while True:
        try:
            with urllib.request.urlopen(url, timeout=1.0) as response:
                return response.status, response.read()
        except OSError:
            if time.monotonic() > deadline:
                raise
            time.sleep(0.05)


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_serve_file(core_data_path, odt_path, tmp_path):
    config = pyodr.HttpServer.Config()
    config.cache_path = str(tmp_path / "server-cache")
    server = pyodr.HttpServer(config)

    file = pyodr.open(str(odt_path))
    html_config = pyodr.HtmlConfig()
    html_config.embed_images = False
    html_config.relative_resource_paths = False
    views = server.serve_file(file, "doc", html_config)
    assert len(views) == 1

    port = free_port()
    thread = threading.Thread(
        target=server.listen, args=("localhost", port), daemon=True
    )
    thread.start()
    try:
        status, body = fetch(f"http://localhost:{port}/file/doc/{views[0].path()}")
        assert status == 200
        assert b"Hello from pyodr!" in body
    finally:
        server.stop()
        thread.join(timeout=5.0)
    assert not thread.is_alive()
