"""Serves a directory of browser checks beside the assets the pages load.

A page links `viewport.js` or `spreadsheet.css` by name; those are not in the
check directory but in `src/odr/internal/html/frontend/`, so what runs here is
the file the library embeds rather than a copy of it. `checks.js` is shared by
every check directory and sits here, which is the second place a name is
looked up.

`error-codes.js` is the third case: the library writes that table into the page
itself (`html/frontend.cpp::write_error_codes`), so there is no file to serve.
It is built from `odr/error_code.{hpp,cpp}` here rather than copied.
"""

import functools
import http.server
import pathlib
import re
import socketserver

ASSETS = (
    pathlib.Path(__file__).resolve().parents[2]
    / "src"
    / "odr"
    / "internal"
    / "html"
    / "frontend"
)


SHARED = pathlib.Path(__file__).resolve().parent

_ERROR_CODE_DIRECTORY = pathlib.Path(__file__).resolve().parents[2] / "src" / "odr"

#: `edit_read_only = 1005` in the header.
_EDIT_VALUE = re.compile(r"^\s*(edit_\w+)\s*=\s*(\d+)\s*,", re.MULTILINE)
#: `{ErrorCode::edit_read_only, "readOnly"}` in the table.
_EDIT_NAME = re.compile(r"\{ErrorCode::(edit_\w+),\s*\"([^\"]+)\"\}")


def _error_codes_js() -> bytes:
    """`odr.errorCodes` as `write_error_codes` writes it."""
    values = dict(
        _EDIT_VALUE.findall((_ERROR_CODE_DIRECTORY / "error_code.hpp").read_text())
    )
    names = _EDIT_NAME.findall((_ERROR_CODE_DIRECTORY / "error_code.cpp").read_text())
    body = ",\n".join(f'  "{name}": {values[code]}' for code, name in names)
    return (
        "window.odr = window.odr || {};\n"
        f"window.odr.errorCodes = {{\n{body}\n}};\n"
    ).encode()


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self) -> None:  # noqa: N802 - the base class spells it this way
        if self.path.split("?")[0].endswith("/error-codes.js"):
            body = _error_codes_js()
            self.send_response(200)
            self.send_header("Content-Type", "text/javascript")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

    def translate_path(self, path: str) -> str:
        translated = pathlib.Path(super().translate_path(path))
        if not translated.is_file():
            for directory in (ASSETS, SHARED):
                candidate = directory / translated.name
                if candidate.is_file():
                    return str(candidate)
        return str(translated)


def serve(port: int, directory: pathlib.Path, pages: tuple[str, ...]) -> None:
    handler = functools.partial(Handler, directory=str(directory))
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", port), handler) as server:
        for page in pages:
            print(f"http://localhost:{port}/{page}")
        server.serve_forever()
