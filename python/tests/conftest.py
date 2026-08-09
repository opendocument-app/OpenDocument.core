import zipfile

import pytest

ODT_CONTENT_XML = """<?xml version="1.0" encoding="UTF-8"?>
<office:document-content
    xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
    xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
    office:version="1.2">
  <office:automatic-styles>
    <text:list-style style:name="Bullets">
      <text:list-level-style-bullet text:level="1" text:bullet-char="•"/>
    </text:list-style>
    <text:list-style style:name="Numbers">
      <text:list-level-style-number text:level="1" style:num-format="1"
          style:num-suffix="."/>
    </text:list-style>
  </office:automatic-styles>
  <office:body>
    <office:text>
      <text:p>Hello from pyodr!</text:p>
      <text:p>Second paragraph</text:p>
      <text:list text:style-name="Bullets">
        <text:list-item><text:p>Bulleted</text:p></text:list-item>
      </text:list>
      <text:list text:style-name="Numbers">
        <text:list-item><text:p>First</text:p></text:list-item>
        <text:list-item><text:p>Second</text:p></text:list-item>
      </text:list>
    </office:text>
  </office:body>
</office:document-content>
"""

ODT_STYLES_XML = """<?xml version="1.0" encoding="UTF-8"?>
<office:document-styles
    xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
    office:version="1.2">
  <office:styles/>
  <office:automatic-styles/>
  <office:master-styles/>
</office:document-styles>
"""

ODT_MANIFEST_XML = """<?xml version="1.0" encoding="UTF-8"?>
<manifest:manifest
    xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0"
    manifest:version="1.2">
  <manifest:file-entry manifest:full-path="/"
      manifest:media-type="application/vnd.oasis.opendocument.text"/>
  <manifest:file-entry manifest:full-path="content.xml"
      manifest:media-type="text/xml"/>
  <manifest:file-entry manifest:full-path="styles.xml"
      manifest:media-type="text/xml"/>
</manifest:manifest>
"""


@pytest.fixture
def odt_path(tmp_path):
    """A minimal OpenDocument text file built from inline XML."""
    path = tmp_path / "minimal.odt"
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr(
            "mimetype",
            "application/vnd.oasis.opendocument.text",
            compress_type=zipfile.ZIP_STORED,
        )
        archive.writestr("content.xml", ODT_CONTENT_XML)
        archive.writestr("styles.xml", ODT_STYLES_XML)
        archive.writestr("META-INF/manifest.xml", ODT_MANIFEST_XML)
    return path


@pytest.fixture
def csv_path(tmp_path):
    path = tmp_path / "table.csv"
    path.write_text("name,value\nalpha,1\nbeta,2\n")
    return path


@pytest.fixture
def txt_path(tmp_path):
    path = tmp_path / "note.txt"
    path.write_text("hello text file\nsecond line\n")
    return path


@pytest.fixture
def json_path(tmp_path):
    path = tmp_path / "data.json"
    path.write_text('{\n  "name": "pyodr",\n  "values": [1, 2, 3]\n}\n')
    return path
