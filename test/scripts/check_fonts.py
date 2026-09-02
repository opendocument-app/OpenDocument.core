#!/usr/bin/env python3
"""Run every font the generated html embeds through OTS.

OTS is the sanitizer every browser puts in front of `@font-face`, and it is far
stricter than FreeType or fontTools: a font it rejects is dropped whole and the
text renders as tofu. Nothing else in the pipeline checks that, and the
comparison against the reference output cannot — a broken font that is already
in the reference is invisible there. See #765.

    check_fonts.py build/test/output

Needs `ots-sanitize`, from `pip install opentype-sanitizer` or on PATH.
"""

import argparse
import base64
import binascii
import hashlib
import os
import re
import subprocess
import sys
import tempfile

# `@font-face{font-family:'odr-f1';src:url(data:font/ttf;base64,AAE...);...}`.
# The rules we write hold no nested braces.
FONT_FACE = re.compile(r"@font-face\{([^}]*)\}")
FONT_FAMILY = re.compile(r"font-family:\s*'([^']*)'")
# Any font payload, whether or not it sits in a face we recognised.
FONT_DATA_URL = re.compile(
    r"data:(?:font/[a-z0-9.+-]+|application/(?:x-)?font[a-z0-9.+-]*)"
    r";base64,([A-Za-z0-9+/=]+)"
)


def find_ots():
    try:
        import ots  # the pip package keeps its binary inside the package

        return [ots.OTS_SANITIZE]
    except ImportError:
        pass
    from shutil import which

    if path := which("ots-sanitize"):
        return [path]
    sys.exit("ots-sanitize not found: pip install opentype-sanitizer")


def html_files(roots):
    for root in roots:
        if os.path.isfile(root):
            yield root
            continue
        for directory, _, names in os.walk(root):
            for name in sorted(names):
                if name.endswith((".html", ".htm")):
                    yield os.path.join(directory, name)


def fonts_in(path):
    """(family, bytes) for every font the file embeds, in document order."""
    with open(path, encoding="utf-8", errors="replace") as file:
        content = file.read()
    seen_at = set()
    for face in FONT_FACE.finditer(content):
        body = face.group(1)
        family = match.group(1) if (match := FONT_FAMILY.search(body)) else "?"
        for data in FONT_DATA_URL.finditer(body):
            seen_at.add(face.start() + data.start())
            yield family, decode(data.group(1), path, family)
    for data in FONT_DATA_URL.finditer(content):
        if data.start() not in seen_at:
            yield "(no @font-face)", decode(data.group(1), path, "?")


def decode(payload, path, family):
    try:
        return base64.b64decode(payload, validate=True)
    except (binascii.Error, ValueError) as error:
        sys.exit(f"{path}: {family}: base64 payload is not decodable: {error}")


def sanitize(ots, font):
    """OTS's complaint, or None if it accepts the font."""
    with tempfile.TemporaryDirectory() as directory:
        source = os.path.join(directory, "font")
        with open(source, "wb") as file:
            file.write(font)
        result = subprocess.run(
            ots + [source, os.path.join(directory, "sanitized")],
            capture_output=True,
            text=True,
            check=False,
        )
    if result.returncode == 0:
        return None
    return (result.stdout + result.stderr).strip() or (
        f"ots-sanitize exited {result.returncode}"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+", help="html file or directory tree")
    arguments = parser.parse_args()

    ots = find_ots()
    # The same font is embedded in every page of a document, so sanitize each
    # distinct payload once and report every place it came from.
    verdicts = {}
    occurrences = {}
    for path in html_files(arguments.roots):
        for family, font in fonts_in(path):
            digest = hashlib.sha256(font).hexdigest()
            if digest not in verdicts:
                verdicts[digest] = sanitize(ots, font)
            occurrences.setdefault(digest, []).append((path, family))

    failed = {d: v for d, v in verdicts.items() if v is not None}
    for digest, complaint in sorted(failed.items()):
        where = occurrences[digest]
        path, family = where[0]
        print(f"FAIL {path}: {family}", file=sys.stderr)
        for line in complaint.splitlines():
            print(f"     {line}", file=sys.stderr)
        if len(where) > 1:
            print(f"     and {len(where) - 1} more embeddings", file=sys.stderr)

    total = sum(len(v) for v in occurrences.values())
    print(
        f"{len(verdicts)} distinct font(s), {total} embedding(s), "
        f"{len(failed)} rejected by OTS"
    )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
