import pytest

import pyodr


def test_file(txt_path):
    file = pyodr.File(str(txt_path))
    assert file
    assert file.location() == pyodr.FileLocation.disk
    assert file.size() == txt_path.stat().st_size
    assert file.disk_path() == str(txt_path)
    assert file.read() == txt_path.read_bytes()


def test_file_from_disk(txt_path):
    file = pyodr.File.from_disk(str(txt_path))
    assert file.location() == pyodr.FileLocation.disk
    assert file.disk_path() == str(txt_path)
    assert file.read() == txt_path.read_bytes()


def test_file_from_memory(txt_path):
    data = txt_path.read_bytes()
    file = pyodr.File.from_memory(data)
    assert file.location() == pyodr.FileLocation.memory
    assert file.disk_path() is None
    assert file.size() == len(data)
    assert file.read() == data


def test_file_from_memory_keeps_bytes_verbatim():
    # not text, and not valid utf-8 - the bytes must survive the round trip
    data = bytes(range(256))
    assert pyodr.File.from_memory(data).read() == data


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


def test_open_from_memory(odt_path):
    file = pyodr.File.from_memory(odt_path.read_bytes())

    assert pyodr.mimetype(file) == "application/vnd.oasis.opendocument.text"
    assert pyodr.FileType.opendocument_text in pyodr.list_file_types(file)

    decoded = pyodr.open(file)
    assert decoded.file_type() == pyodr.FileType.opendocument_text
    assert decoded.is_document_file()
    # the bytes are the only copy there is, so decoding has to have kept them
    document = decoded.as_document_file().document()
    assert document.document_type() == pyodr.DocumentType.text


def test_open_from_memory_as_type(odt_path):
    file = pyodr.File.from_memory(odt_path.read_bytes())

    assert pyodr.open(file, pyodr.FileType.zip).is_archive_file()

    preference = pyodr.DecodePreference()
    preference.as_file_type = pyodr.FileType.zip
    assert pyodr.open(file, preference).is_archive_file()


def test_decoded_file_from_file(odt_path):
    file = pyodr.File.from_memory(odt_path.read_bytes())

    assert pyodr.DecodedFile(file).file_type() == pyodr.FileType.opendocument_text
    assert pyodr.DecodedFile(file, pyodr.FileType.zip).is_archive_file()

    preference = pyodr.DecodePreference()
    preference.as_file_type = pyodr.FileType.zip
    assert pyodr.DecodedFile(file, preference).is_archive_file()


def test_document_file_from_file(odt_path):
    file = pyodr.File.from_memory(odt_path.read_bytes())

    assert pyodr.DocumentFile.type_by_file(file) == pyodr.FileType.opendocument_text
    assert (
        pyodr.DocumentFile.meta_by_file(file).type == pyodr.FileType.opendocument_text
    )

    document_file = pyodr.DocumentFile(file)
    assert document_file.document_type() == pyodr.DocumentType.text


def test_document_file_from_disk_and_from_memory(odt_path):
    from_disk = pyodr.DocumentFile.from_disk(str(odt_path))
    from_memory = pyodr.DocumentFile.from_memory(odt_path.read_bytes())

    assert from_disk.file_type() == pyodr.FileType.opendocument_text
    assert from_memory.file_type() == from_disk.file_type()
    assert from_memory.document_type() == from_disk.document_type()
    assert (
        from_memory.document().document_type() == from_disk.document().document_type()
    )


def test_document_file_from_memory_rejects_a_non_document():
    with pytest.raises(pyodr.Error):
        pyodr.DocumentFile.from_memory(b"not a document")


def test_file_and_path_entry_points_agree(odt_path):
    path = str(odt_path)
    file = pyodr.File.from_disk(path)

    assert pyodr.mimetype(file) == pyodr.mimetype(path)
    assert pyodr.list_file_types(file) == pyodr.list_file_types(path)
    assert pyodr.open(file).file_type() == pyodr.open(path).file_type()
    assert pyodr.DocumentFile.type_by_file(file) == pyodr.DocumentFile.type_by_path(
        path
    )
    assert (
        pyodr.DocumentFile.meta_by_file(file).type
        == pyodr.DocumentFile.meta_by_path(path).type
    )
