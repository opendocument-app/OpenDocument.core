#!/usr/bin/env python3
"""Replay check-editing.cjs's export, then compare saved/reopened text and style."""
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / "cmake-build-relwithdebinfo" / "python"))
import pyodr


def walk(element):
    yield element
    for child in element.children():
        yield from walk(child)


def snapshot(document):
    elements = list(walk(document.root_element()))
    paragraphs = [e for e in elements if e.type() == pyodr.ElementType.paragraph]
    runs = [e.as_text() for e in elements if e.type() == pyodr.ElementType.text]
    return {
        "paragraphs": [
            "".join(
                e.as_text().content()
                for e in walk(p)
                if e.type() == pyodr.ElementType.text
            )
            for p in paragraphs
        ],
        "runs": [
            {
                "text": run.content(),
                "bold": run.style().font_weight == pyodr.FontWeight.bold,
            }
            for run in runs
        ],
    }


expected = json.loads(Path(sys.argv[1]).read_text())
document = pyodr.open(str(HERE / "demo-document.fodt")).as_document_file().document()
document.edit(json.dumps(expected["operations"], ensure_ascii=False))
expected = {key: expected[key] for key in ("paragraphs", "runs")}
assert snapshot(document) == expected, "core replay differs from browser source"
saved = document.save_to_memory()
reopened = pyodr.open(pyodr.File.from_memory(saved)).as_document_file().document()
assert (
    snapshot(reopened) == expected
), "save/reopen changed logical text, structure or style"
print(
    "PASS browser edits replay into core and survive save/reopen with matching paragraphs and styles"
)
