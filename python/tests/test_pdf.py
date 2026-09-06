import json

import pytest

import pyodr


def annotate(pdf_path, annotations):
    return pyodr.open(str(pdf_path)).as_pdf_file().annotate(json.dumps(annotations))


def test_annotate_is_declared_for_pdf():
    capabilities = pyodr.capabilities_by_file_type(
        pyodr.FileType.portable_document_format
    )
    assert capabilities.annotate


def test_annotate_appends_to_the_source(pdf_path):
    source = pdf_path.read_bytes()
    result = annotate(
        pdf_path,
        {
            "version": 1,
            "annotations": [
                {
                    "page": 0,
                    "type": "highlight",
                    "quads": [[72, 700, 300, 700, 72, 688, 300, 688]],
                    "color": [1, 0.9, 0.2],
                }
            ],
        },
    )

    assert isinstance(result, bytes)
    # the source is copied and the annotation appended after it
    assert result.startswith(source)
    assert b"/Highlight" in result
    assert b"/Subtype /Form" in result


def test_annotate_writes_ink(pdf_path):
    result = annotate(
        pdf_path,
        {
            "version": 1,
            "annotations": [
                {
                    "page": 0,
                    "type": "ink",
                    "strokes": [[100, 500, 130, 540, 160, 490]],
                    "width": 2,
                    "color": [0.9, 0.1, 0.1],
                }
            ],
        },
    )
    assert b"/Ink" in result
    assert b"/InkList" in result


def test_annotate_refuses_a_payload_it_does_not_understand(pdf_path):
    with pytest.raises(ValueError):
        annotate(pdf_path, {"version": 2, "annotations": []})
    with pytest.raises(ValueError):
        annotate(
            pdf_path,
            {"version": 1, "annotations": [{"page": 9, "type": "ink"}]},
        )
