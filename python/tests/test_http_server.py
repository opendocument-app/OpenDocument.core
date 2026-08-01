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
def test_serve_file(odt_path, tmp_path):
    server = pyodr.HttpServer()

    # the server hosts what it is given; translating is the caller's business
    cache_path = tmp_path / "doc-cache"
    cache_path.mkdir()
    file = pyodr.open(str(odt_path))
    html_config = pyodr.HtmlConfig()
    html_config.embed_images = False
    service = pyodr.html.translate(file, str(cache_path), html_config)
    server.connect_service(service, "doc")
    views = service.list_views()
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
def test_bind_reports_what_it_got():
    server = pyodr.HttpServer()

    port = server.bind("127.0.0.1", 0)
    assert port != 0

    # a second bind would leak the first socket, so it is refused
    with pytest.raises(RuntimeError):
        server.bind("127.0.0.1", 0)

    server.stop()


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_bind_reports_a_port_in_use():
    taken = pyodr.HttpServer()
    # a literal address on purpose: "localhost" resolves to both ::1 and
    # 127.0.0.1, so a second bind lands on the other one instead of colliding
    port = taken.bind("127.0.0.1", 0)

    other = pyodr.HttpServer()
    # reuse_port defaults off, or the two would share the port
    with pytest.raises(RuntimeError):
        other.bind("127.0.0.1", port)

    taken.stop()


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_listen_without_bind_raises():
    server = pyodr.HttpServer()

    # cpp-httplib reports success for this, hence the guard being tested
    with pytest.raises(RuntimeError):
        server.listen()


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_stop_waits_for_listen():
    server = pyodr.HttpServer()
    server.bind("127.0.0.1", 0)

    thread = threading.Thread(target=server.listen, daemon=True)
    thread.start()
    while not server.is_running():
        time.sleep(0.001)

    server.stop()
    # the accept loop is gone for good by now, which is what makes dropping the
    # server right after safe
    assert not server.is_running()

    thread.join(timeout=5.0)
    assert not thread.is_alive()


@pytest.mark.skipif(not pyodr.has_http_server, reason="built without the HTTP server")
def test_listen_after_stop_returns():
    server = pyodr.HttpServer()
    server.bind("127.0.0.1", 0)
    server.stop()

    # a listen thread that starts late finds the server stopped; there is
    # nothing to serve, but nothing went wrong either
    server.listen()
