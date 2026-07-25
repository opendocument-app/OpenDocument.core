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


def test_decoder_engine():
    assert pyodr.decoder_engine_by_name("odr") == pyodr.DecoderEngine.odr
    assert pyodr.decoder_engine_to_string(pyodr.DecoderEngine.odr) == "odr"


def test_global_params():
    assert isinstance(pyodr.GlobalParams.odr_core_data_path(), str)
