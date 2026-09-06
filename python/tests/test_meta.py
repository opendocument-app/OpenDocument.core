import pyodr


def test_version():
    # A dev build carries no project version; only assert consistency.
    assert isinstance(pyodr.version(), str)
    assert pyodr.__version__ == pyodr.version()


def test_commit_hash():
    assert isinstance(pyodr.commit_hash(), str)


def test_identify():
    assert pyodr.identify()


def test_file_type_by_file_extension():
    assert pyodr.file_type_by_file_extension("odt") == pyodr.FileType.opendocument_text
    assert (
        pyodr.file_type_by_file_extension("docx")
        == pyodr.FileType.office_open_xml_document
    )
    assert (
        pyodr.file_type_by_file_extension("pdf")
        == pyodr.FileType.portable_document_format
    )
    assert pyodr.file_type_by_file_extension("nope") == pyodr.FileType.unknown


def test_file_category_by_file_type():
    assert (
        pyodr.file_category_by_file_type(pyodr.FileType.opendocument_text)
        == pyodr.FileCategory.document
    )
    assert (
        pyodr.file_category_by_file_type(pyodr.FileType.zip)
        == pyodr.FileCategory.archive
    )


def test_document_type_by_file_type():
    assert (
        pyodr.document_type_by_file_type(pyodr.FileType.opendocument_spreadsheet)
        == pyodr.DocumentType.spreadsheet
    )


def test_type_to_string():
    assert pyodr.file_type_to_string(pyodr.FileType.opendocument_text)
    assert pyodr.file_category_to_string(pyodr.FileCategory.document)
    assert pyodr.document_type_to_string(pyodr.DocumentType.text)


def test_mimetype_roundtrip():
    mimetype = pyodr.mimetype_by_file_type(pyodr.FileType.portable_document_format)
    assert mimetype == "application/pdf"
    assert (
        pyodr.file_type_by_mimetype(mimetype) == pyodr.FileType.portable_document_format
    )


def test_all_file_types_and_aliases_round_trip():
    file_types = pyodr.all_file_types()
    assert pyodr.FileType.opendocument_text in file_types

    extensions = set()
    mimetypes = set()
    for file_type in file_types:
        for extension in pyodr.file_extensions_by_file_type(file_type):
            assert extension not in extensions
            extensions.add(extension)
            assert pyodr.file_type_by_file_extension(extension) == file_type
        for mimetype in pyodr.mimetypes_by_file_type(file_type):
            assert mimetype not in mimetypes
            mimetypes.add(mimetype)
            assert pyodr.file_type_by_mimetype(mimetype) == file_type

    # the aliases the issue asked for
    assert pyodr.file_type_by_file_extension("docm") == (
        pyodr.FileType.office_open_xml_document
    )
    # its own type: an OOXML package we deliberately cannot open
    assert pyodr.file_type_by_file_extension("xlsb") == (
        pyodr.FileType.excel_binary_workbook
    )
    assert not pyodr.capabilities_by_file_type(
        pyodr.FileType.excel_binary_workbook
    ).open
    assert pyodr.file_type_by_mimetype("application/x-vnd.oasis.opendocument.text") == (
        pyodr.FileType.opendocument_text
    )


def test_capabilities_by_file_type():
    odt = pyodr.capabilities_by_file_type(pyodr.FileType.opendocument_text)
    assert odt.open
    assert odt.translate_html
    assert odt.color_scheme
    assert odt.edit

    # detected and named, but there is no decoder behind it
    wpd = pyodr.capabilities_by_file_type(pyodr.FileType.word_perfect)
    assert wpd.detect_by_content
    assert not wpd.open
    assert not wpd.translate_html

    # a sheet cell can be written, and the package written back
    ods = pyodr.capabilities_by_file_type(pyodr.FileType.opendocument_spreadsheet)
    assert ods.edit
    assert ods.save

    # a pdf renders, but paints its own page backgrounds
    pdf = pyodr.capabilities_by_file_type(pyodr.FileType.portable_document_format)
    assert pdf.translate_html
    assert not pdf.color_scheme


def test_decoded_file_capabilities(odt_path):
    capabilities = pyodr.open(str(odt_path)).capabilities()
    assert capabilities.open
    assert capabilities.translate_html
    # not encrypted, so there is nothing to decrypt
    assert not capabilities.decrypt


def test_mimetype_names_what_is_inside_the_container(odt_path):
    # An ODF file is a ZIP, and the answer worth having is the one from inside
    # it. Detection opens the container to get there.
    assert pyodr.mimetype(str(odt_path)) == "application/vnd.oasis.opendocument.text"


def test_text_encoding_lookups():
    assert pyodr.TextEncoding.utf8 in pyodr.all_text_encodings()
    # `unknown` is the one with no name, and is left out
    assert pyodr.TextEncoding.unknown not in pyodr.all_text_encodings()

    assert (
        pyodr.text_encoding_to_string(pyodr.TextEncoding.windows_1252) == "windows-1252"
    )
    # case and separators are ignored, so an alias resolves
    assert pyodr.text_encoding_by_name("CP1252") == pyodr.TextEncoding.windows_1252
    assert pyodr.text_encoding_by_name("nope") == pyodr.TextEncoding.unknown
    assert "windows-1252" in pyodr.text_encoding_names(pyodr.TextEncoding.windows_1252)

    assert pyodr.text_encoding_is_decodable(pyodr.TextEncoding.utf8)
    # named so a caller can say what a file is, but not decoded here
    assert not pyodr.text_encoding_is_decodable(pyodr.TextEncoding.shift_jis)


def test_text_file_reports_its_encoding(tmp_path):
    path = tmp_path / "plain.txt"
    path.write_text("hello", encoding="utf-8")

    text_file = pyodr.open(str(path)).as_text_file()
    assert text_file.encoding() != pyodr.TextEncoding.unknown
    assert isinstance(text_file.charset(), str)
