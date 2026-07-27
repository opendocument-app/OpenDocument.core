import pytest

import pyodr


def test_file(txt_path):
    file = pyodr.File(str(txt_path))
    assert file
    assert file.location() == pyodr.FileLocation.disk
    assert file.size() == txt_path.stat().st_size
    assert file.disk_path() == str(txt_path)
    assert file.read() == txt_path.read_bytes()


def test_open_missing_file(tmp_path):
    with pytest.raises(FileNotFoundError):
        pyodr.open(str(tmp_path / "missing.txt"))


def test_open_text_file(txt_path):
    file = pyodr.open(str(txt_path))
    assert file.file_type() == pyodr.FileType.text_file
    assert file.file_category() == pyodr.FileCategory.text
    assert file.is_text_file()
    assert not file.is_document_file()

    text_file = file.as_text_file()
    assert "hello text file" in text_file.text()


def test_open_csv_file(csv_path):
    file = pyodr.open(str(csv_path))
    assert file.file_type() == pyodr.FileType.comma_separated_values

    file_types = pyodr.list_file_types(str(csv_path))
    assert pyodr.FileType.comma_separated_values in file_types


def test_open_json_file(json_path):
    file = pyodr.open(str(json_path))
    assert file.file_type() == pyodr.FileType.javascript_object_notation


def test_open_as_type(txt_path):
    file = pyodr.open(str(txt_path), pyodr.FileType.text_file)
    assert file.file_type() == pyodr.FileType.text_file


def test_open_with_preference(txt_path):
    preference = pyodr.DecodePreference()
    preference.as_file_type = pyodr.FileType.text_file
    file = pyodr.open(str(txt_path), preference)
    assert file.file_type() == pyodr.FileType.text_file


def test_file_meta(csv_path):
    file = pyodr.open(str(csv_path))
    meta = file.file_meta()
    assert meta.type == pyodr.FileType.comma_separated_values
    assert not meta.password_encrypted
    # not a document, so the document fields stay unset
    assert meta.document_type == pyodr.DocumentType.unknown
    assert meta.entry_count is None
    assert meta.title is None


def test_open_zip_archive(odt_path):
    file = pyodr.open(str(odt_path), pyodr.FileType.zip)
    assert file.is_archive_file()

    filesystem = file.as_archive_file().archive().as_filesystem()
    assert filesystem.is_file("/mimetype")
    assert filesystem.is_file("/content.xml")
    assert not filesystem.exists("/nonexistent")

    mimetype = filesystem.open("/mimetype").read()
    assert mimetype == b"application/vnd.oasis.opendocument.text"
