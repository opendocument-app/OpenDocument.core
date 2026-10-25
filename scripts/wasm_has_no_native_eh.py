#!/usr/bin/env python3
"""Fails where an emscripten script carries wasm with native exceptions.

Native wasm exceptions live in a tag section (id 13), and a browser before
Chrome 95 refuses a module that has one. The formula module that the native
builds embed is built with exceptions in JavaScript (`formula.yml`), so it
must have none. The wasm sits in the script as one base64 data url
(`-sSINGLE_FILE`).

    scripts/wasm_has_no_native_eh.py build/wasm/formula/odr-formula.js
"""

import base64
import re
import sys

TAG_SECTION = 13


def sections(wasm: bytes) -> list[int]:
    """The id of each section, after the magic number and the version."""
    result = []
    at = 8
    while at < len(wasm):
        section = wasm[at]
        at += 1
        size = 0
        shift = 0
        while True:
            byte = wasm[at]
            at += 1
            size |= (byte & 0x7F) << shift
            shift += 7
            if byte < 0x80:
                break
        result.append(section)
        at += size
    return result


def main() -> int:
    script = open(sys.argv[1], encoding="utf-8").read()
    match = re.search(r"data:application/octet-stream;base64,([A-Za-z0-9+/=]+)", script)
    if match is None:
        print(f"{sys.argv[1]}: no wasm in it", file=sys.stderr)
        return 1
    wasm = base64.b64decode(match.group(1))
    found = sections(wasm)
    if TAG_SECTION in found:
        print(f"{sys.argv[1]}: uses native wasm exceptions", file=sys.stderr)
        return 1
    print(f"{sys.argv[1]}: {len(wasm)} bytes of wasm, no native exceptions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
