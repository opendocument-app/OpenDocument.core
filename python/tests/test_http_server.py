import threading
import time
import urllib.request

import pytest

import pyodr


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

    port = server.bind("localhost", 0)
    thread = threading.Thread(target=server.listen, daemon=True)
    thread.start()
    try:
        status, body = fetch(f"http://localhost:{port}/file/doc/{views[0].path()}")
        assert status == 200
        assert b"Hello from pyodr!" in body
    finally:
        server.stop()
        thread.join(timeout=5.0)
    assert not thread.is_alive()


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_bind_reports_what_it_got(tmp_path):
    config = pyodr.HttpServer.Config()
    cache_path = tmp_path / "server-cache"
    cache_path.mkdir()
    config.cache_path = str(cache_path)
    server = pyodr.HttpServer(config)

    port = server.bind("127.0.0.1", 0)
    assert port != 0

    # a second bind would leak the first socket, so it is refused
    with pytest.raises(RuntimeError):
        server.bind("127.0.0.1", 0)

    server.stop()


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_bind_reports_a_port_in_use(tmp_path):
    config = pyodr.HttpServer.Config()
    cache_path = tmp_path / "server-cache"
    cache_path.mkdir()
    config.cache_path = str(cache_path)
    taken = pyodr.HttpServer(config)
    # a literal address on purpose: "localhost" resolves to both ::1 and
    # 127.0.0.1, so a second bind lands on the other one instead of colliding
    port = taken.bind("127.0.0.1", 0)

    other = pyodr.HttpServer(config)
    options = pyodr.HttpServer.Options()
    options.reuse_port = False  # or the two would share the port
    with pytest.raises(RuntimeError):
        other.bind("127.0.0.1", port, options)

    taken.stop()


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_listen_without_bind_raises(tmp_path):
    config = pyodr.HttpServer.Config()
    cache_path = tmp_path / "server-cache"
    cache_path.mkdir()
    config.cache_path = str(cache_path)
    server = pyodr.HttpServer(config)

    # cpp-httplib reports success for this, hence the guard being tested
    with pytest.raises(RuntimeError):
        server.listen()
