import zipfile

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


def test_file_name(txt_path):
    assert pyodr.File.from_disk(str(txt_path)).name() == "note.txt"
    # bytes arrive unnamed unless the caller says otherwise
    assert pyodr.File.from_memory(b"hello").name() == ""

    named = pyodr.File.from_memory(b"hello", "greeting.txt")
    assert named.name() == "greeting.txt"


def test_file_name_of_an_archive_entry(odt_path):
    file = pyodr.open(
        str(odt_path), pyodr.DecodeOptions(as_file_type=pyodr.FileType.zip)
    )
    filesystem = file.as_archive_file().archive().as_filesystem()
    assert filesystem.open("/META-INF/manifest.xml").name() == "manifest.xml"


def test_named_bytes_decode_as_markdown():
    # markdown has no signature, so only the name can offer it
    named = pyodr.File.from_memory(b"# heading\n", "notes.md")
    assert pyodr.open(named).file_type() == pyodr.FileType.markdown

    unnamed = pyodr.File.from_memory(b"# heading\n")
    assert pyodr.open(unnamed).file_type() == pyodr.FileType.text_file


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
    file = pyodr.open(
        str(txt_path), pyodr.DecodeOptions(as_file_type=pyodr.FileType.text_file)
    )
    assert file.file_type() == pyodr.FileType.text_file


def test_open_with_options(txt_path):
    options = pyodr.DecodeOptions(as_file_type=pyodr.FileType.text_file)
    file = pyodr.open(str(txt_path), options)
    assert file.file_type() == pyodr.FileType.text_file


def test_open_carries_csv_options(tmp_path):
    path = tmp_path / "semicolons.csv"
    path.write_text("a;b\n1;2\n", encoding="utf-8")

    # detection would find the semicolon; a pipe it would not, so the caller says
    csv = pyodr.open(
        str(path),
        pyodr.DecodeOptions(
            as_file_type=pyodr.FileType.comma_separated_values,
            csv=pyodr.CsvOptions(separator="|"),
        ),
    ).as_csv_file()
    assert csv.options().separator == "|"


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
    file = pyodr.open(
        str(odt_path), pyodr.DecodeOptions(as_file_type=pyodr.FileType.zip)
    )
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

    assert pyodr.open(
        file, pyodr.DecodeOptions(as_file_type=pyodr.FileType.zip)
    ).is_archive_file()

    options = pyodr.DecodeOptions(as_file_type=pyodr.FileType.zip)
    assert pyodr.open(file, options).is_archive_file()


def test_decoded_file_from_file(odt_path):
    file = pyodr.File.from_memory(odt_path.read_bytes())

    assert pyodr.open(file).file_type() == pyodr.FileType.opendocument_text
    assert pyodr.open(
        file, pyodr.DecodeOptions(as_file_type=pyodr.FileType.zip)
    ).is_archive_file()

    options = pyodr.DecodeOptions(as_file_type=pyodr.FileType.zip)
    assert pyodr.open(file, options).is_archive_file()


def test_document_file_from_file(odt_path):
    file = pyodr.File.from_memory(odt_path.read_bytes())

    document_file = pyodr.open(file).as_document_file()
    assert document_file.file_type() == pyodr.FileType.opendocument_text
    assert document_file.file_meta().type == pyodr.FileType.opendocument_text

    assert document_file.document_type() == pyodr.DocumentType.text


def test_document_file_from_disk_and_from_memory(odt_path):
    from_disk = pyodr.open(str(odt_path)).as_document_file()
    from_memory = pyodr.open(
        pyodr.File.from_memory(odt_path.read_bytes())
    ).as_document_file()

    assert from_disk.file_type() == pyodr.FileType.opendocument_text
    assert from_memory.file_type() == from_disk.file_type()
    assert from_memory.document_type() == from_disk.document_type()
    assert (
        from_memory.document().document_type() == from_disk.document().document_type()
    )


def test_document_file_thumbnail(tmp_path, odt_path):
    # The minimal odt the fixture builds carries none.
    assert pyodr.open(str(odt_path)).as_document_file().thumbnail() is None

    with_thumbnail = tmp_path / "with-thumbnail.odt"
    with zipfile.ZipFile(odt_path) as source:
        entries = {name: source.read(name) for name in source.namelist()}
    entries["Thumbnails/thumbnail.png"] = b"not really a png"
    with zipfile.ZipFile(with_thumbnail, "w") as archive:
        for name, content in entries.items():
            archive.writestr(name, content)

    thumbnail = pyodr.open(str(with_thumbnail)).as_document_file().thumbnail()
    assert thumbnail is not None
    assert thumbnail.read() == b"not really a png"


def test_document_file_from_memory_rejects_a_non_document():
    with pytest.raises(pyodr.Error):
        pyodr.open(pyodr.File.from_memory(b"not a document")).as_document_file()


def test_file_and_path_entry_points_agree(odt_path):
    path = str(odt_path)
    file = pyodr.File.from_disk(path)

    assert pyodr.mimetype(file) == pyodr.mimetype(path)
    assert pyodr.list_file_types(file) == pyodr.list_file_types(path)
    assert pyodr.open(file).file_type() == pyodr.open(path).file_type()
    assert (
        pyodr.open(file).as_document_file().file_type()
        == pyodr.open(path).as_document_file().file_type()
    )


def test_text_file_writes_an_edit_back(txt_path):
    text_file = pyodr.open(str(txt_path)).as_text_file()
    assert text_file.is_savable()

    edited = text_file.write_edited(
        '{"version":2,"ops":[{"op":"setContent","text":"rewritten\\n"}]}'
    )
    assert edited == b"rewritten\n"


def test_a_csv_holds_a_text_file_rather_than_being_one(csv_path):
    file = pyodr.open(str(csv_path))
    assert not file.is_text_file()
    assert file.is_csv_file()
    assert file.as_csv_file().text_file().text().startswith("name,value")
