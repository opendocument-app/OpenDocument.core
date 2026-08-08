"""Command line interface: render a document to HTML and open or serve it."""

import argparse
import signal
import sys
import tempfile
import webbrowser
from pathlib import Path
from typing import Optional, Sequence

import pyodr


def _open_decoded_file(path: Path, password: Optional[str]):
    try:
        file = pyodr.open(str(path.resolve()))
    except FileNotFoundError:
        print(f"file not found: {path}", file=sys.stderr)
        return None
    except pyodr.UnknownFileTypeError:
        print(f"unknown file type: {path}", file=sys.stderr)
        return None

    print(f"file type: {pyodr.file_type_to_string(file.file_type())}")

    if file.password_encrypted():
        if password is None:
            print("file is encrypted, use --password", file=sys.stderr)
            return None
        try:
            file = file.decrypt(password)
        except pyodr.WrongPasswordError:
            print("wrong password", file=sys.stderr)
            return None

    return file


def _translate(args, file) -> int:
    if args.output is not None:
        output = args.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
    else:
        output = Path(tempfile.mkdtemp(prefix="pyodr-"))
    cache = tempfile.mkdtemp(prefix="pyodr-cache-")

    service = pyodr.html.translate(file, cache, pyodr.HtmlConfig())
    html = service.bring_offline(str(output))

    for page in html.pages():
        print(f"{page.name}: {page.path}")
        if not args.no_open:
            webbrowser.open(f"file://{page.path}")

    return 0


def _serve(args, file) -> int:
    if not pyodr.has_http_server:
        print("pyodr was built without the HTTP server", file=sys.stderr)
        return 1

    html_config = pyodr.HtmlConfig()
    html_config.embed_images = False
    cache = tempfile.mkdtemp(prefix="pyodr-server-")
    service = pyodr.html.translate(file, cache, html_config)

    prefix = "file"
    server = pyodr.HttpServer()
    server.connect_service(service, prefix)
    # bind before printing: the port is only known once the socket is, and it is
    # not necessarily the one that was asked for
    port = server.bind(args.host, args.port)
    urls = [
        f"http://{args.host}:{port}/file/{prefix}/{view.path()}"
        for view in service.list_views()
    ]
    for url in urls:
        print(url)
    if not args.no_open and urls:
        webbrowser.open(urls[0])

    # `listen` blocks in C++; restore the default SIGINT handler so Ctrl+C
    # terminates the server.
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    server.listen()

    return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        prog="pyodr", description="OpenDocument Reader: render documents to HTML"
    )
    parser.add_argument("file", type=Path, help="the file to open")
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=None,
        help="output directory (default: a temporary directory)",
    )
    parser.add_argument(
        "--password", "-p", default=None, help="password for encrypted files"
    )
    parser.add_argument(
        "--no-open",
        action="store_true",
        help="do not open the result in a web browser",
    )
    parser.add_argument(
        "--serve",
        "-s",
        action="store_true",
        help="serve the file over HTTP instead of writing files",
    )
    parser.add_argument(
        "--host", default="localhost", help="HTTP server host (with --serve)"
    )
    parser.add_argument(
        "--port", type=int, default=8080, help="HTTP server port (with --serve)"
    )
    args = parser.parse_args(argv)

    file = _open_decoded_file(args.file, args.password)
    if file is None:
        return 1

    if args.serve:
        return _serve(args, file)
    return _translate(args, file)


if __name__ == "__main__":
    sys.exit(main())
