#!/usr/bin/env python3
"""Compare the text of the generated pdf pages with the text ghostscript reads.

The text layer (`.sel`) is what search, selection and copy see. A wrong
`ToUnicode` or glyph-name mapping changes it and leaves the layout alone, so the
screenshot comparison does not notice, and a reference output that already
holds the bug hides it. `gs -sDEVICE=txtwrite` reads the source pdf on its own,
so it does not depend on the reference output. See #765.

    check_pdf_text.py test/data/input build/test/output

For each pdf, the check counts the words of four or more letters that
ghostscript reads, and reports the share of them that the html also holds
(the recall). A file fails where its recall drops below its floor in
`pdf_text_floors.json`, or below the default floor there. gs is wrong on some
files, and the floor of such a file says why.

Needs `gs` on PATH.
"""

import argparse
import collections
import html.parser
import json
import os
import re
import shutil
import subprocess
import sys

WORD = re.compile(r"[^\W\d_]{4,}")
# no end tag follows these, so they must not count as a level of the layer
VOID_ELEMENTS = {"area", "br", "col", "embed", "hr", "img", "input", "wbr"}
# gs reads the whole corpus in about a minute
GS_TIMEOUT_SECONDS = 300
FLOORS = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "pdf_text_floors.json"
)


class TextLayer(html.parser.HTMLParser):
    """The text inside every `class="sel"` element."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.depth = 0
        self.parts = []

    def handle_starttag(self, tag, attrs):
        if tag in VOID_ELEMENTS:
            if tag == "br" and self.depth > 0:
                self.parts.append(" ")
            return
        if self.depth > 0:
            self.depth += 1
        elif "sel" in (dict(attrs).get("class") or "").split():
            self.depth = 1

    def handle_endtag(self, tag):
        if tag not in VOID_ELEMENTS and self.depth > 0:
            self.depth -= 1

    def handle_data(self, data):
        if self.depth > 0:
            self.parts.append(data)


def words(text):
    return collections.Counter(word.lower() for word in WORD.findall(text))


def pages(directory):
    return [
        name for name in os.listdir(directory) if re.fullmatch(r"page\d+\.html", name)
    ]


def html_words(directory):
    layer = TextLayer()
    for name in sorted(pages(directory)):
        with open(os.path.join(directory, name), encoding="utf-8") as file:
            layer.feed(file.read())
        # pages do not run into each other
        layer.parts.append(" ")
    return words("".join(layer.parts))


def gs_words(gs, path, page_count):
    """The words of the first @p page_count pages, which are the ones
    `odr_test` renders (`page_range_end`)."""
    result = subprocess.run(
        [
            gs,
            "-q",
            "-dNOPAUSE",
            "-dBATCH",
            "-dSAFER",
            "-sstdout=%stderr",
            "-sDEVICE=txtwrite",
            "-dFirstPage=1",
            f"-dLastPage={page_count}",
            "-sOutputFile=-",
            path,
        ],
        capture_output=True,
        check=False,
        timeout=GS_TIMEOUT_SECONDS,
    )
    if result.returncode != 0:
        return None
    return words(result.stdout.decode("utf-8", errors="replace"))


def recall(expected, actual):
    total = sum(expected.values())
    if total == 0:
        return None
    found = sum(min(count, actual[word]) for word, count in expected.items())
    return found / total


def pairs(input_root, output_root):
    """(name, source pdf, output directory) for each pdf both trees hold."""
    for repository in sorted(os.listdir(output_root)):
        outputs = os.path.join(output_root, repository, "output", "pdf")
        inputs = os.path.join(input_root, repository, "pdf")
        if not os.path.isdir(outputs) or not os.path.isdir(inputs):
            continue
        for name in sorted(os.listdir(outputs)):
            source = os.path.join(inputs, name)
            if os.path.isfile(source):
                yield f"{repository}/pdf/{name}", source, os.path.join(outputs, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="test/data/input")
    parser.add_argument("output", help="the output tree odr_test writes")
    parser.add_argument(
        "--report", action="store_true", help="print the recall of every file"
    )
    arguments = parser.parse_args()

    gs = shutil.which("gs")
    if gs is None:
        sys.exit("gs not found: brew install ghostscript")
    with open(FLOORS, encoding="utf-8") as file:
        floors = json.load(file)
    default = floors["default"]

    checked = 0
    failed = []
    for name, source, output in pairs(arguments.input, arguments.output):
        page_count = len(pages(output))
        if page_count == 0:
            continue
        try:
            expected = gs_words(gs, source, page_count)
        except subprocess.TimeoutExpired:
            failed.append((name, f"gs gave no answer within {GS_TIMEOUT_SECONDS} s"))
            continue
        if expected is None:
            if arguments.report:
                print(f"skip  {name}: gs cannot read it")
            continue
        share = recall(expected, html_words(output))
        if share is None:
            if arguments.report:
                print(f"skip  {name}: gs reads no words")
            continue
        checked += 1
        floor = floors["files"].get(name, {}).get("floor", default)
        if arguments.report:
            print(f"{share:6.1%} {name} ({sum(expected.values())} words)")
        if share < floor:
            failed.append((name, f"recall {share:.1%}, floor {floor:.1%}"))

    if checked == 0:
        sys.exit("No pdf output found to check")
    for name, reason in failed:
        print(f"FAIL {name}: {reason}", file=sys.stderr)
    print(f"{checked} pdf file(s) checked, {len(failed)} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
