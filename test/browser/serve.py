"""Serves a directory of browser checks beside the assets the pages load.

A page links `viewport.js` or `spreadsheet.css` by name; those are not in the
check directory but in `src/odr/internal/html/frontend/`, so what runs here is
the file the library embeds rather than a copy of it.
"""

import functools
import http.server
import pathlib
import socketserver

ASSETS = (
    pathlib.Path(__file__).resolve().parents[2]
    / "src"
    / "odr"
    / "internal"
    / "html"
    / "frontend"
)


class Handler(http.server.SimpleHTTPRequestHandler):
    def translate_path(self, path: str) -> str:
        translated = pathlib.Path(super().translate_path(path))
        if not translated.is_file():
            asset = ASSETS / translated.name
            if asset.is_file():
                return str(asset)
        return str(translated)


def serve(port: int, directory: pathlib.Path, pages: tuple[str, ...]) -> None:
    handler = functools.partial(Handler, directory=str(directory))
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", port), handler) as server:
        for page in pages:
            print(f"http://localhost:{port}/{page}")
        server.serve_forever()
