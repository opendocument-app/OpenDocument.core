import pyodr
import pytest


def walk_text(element):
    """Collect the text content of an element subtree."""
    parts = []
    if element.type() == pyodr.ElementType.text:
        parts.append(element.as_text().content())
    for child in element.children():
        parts.extend(walk_text(child))
    return parts


def test_open_odt(odt_path):
    file = pyodr.open(str(odt_path))
    assert file.file_type() == pyodr.FileType.opendocument_text
    assert file.file_category() == pyodr.FileCategory.document
    assert file.is_document_file()

    document_file = file.as_document_file()
    assert document_file.document_type() == pyodr.DocumentType.text
    assert not document_file.password_encrypted()


def test_document_meta(odt_path):
    meta = pyodr.open(str(odt_path)).as_document_file().file_meta()
    assert meta.document_type == pyodr.DocumentType.text


def test_element_tree(odt_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    assert document.document_type() == pyodr.DocumentType.text
    assert document.file_type() == pyodr.FileType.opendocument_text

    root = document.root_element()
    assert root
    assert root.type() == pyodr.ElementType.root

    children = list(root.children())
    paragraphs = [
        child for child in children if child.type() == pyodr.ElementType.paragraph
    ]
    assert len(paragraphs) == 2

    text = walk_text(root)
    assert "Hello from pyodr!" in text
    assert "Second paragraph" in text


def test_element_navigation(odt_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    root = document.root_element()

    first = root.first_child()
    assert first
    assert first.parent() == root
    second = first.next_sibling()
    assert second
    assert second.previous_sibling() == first


def test_failed_cast_is_falsy(odt_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    paragraph = next(
        child
        for child in document.root_element()
        if child.type() == pyodr.ElementType.paragraph
    )

    assert paragraph.as_paragraph()
    # the wrong cast has to come back falsy, not as a valid-looking handle
    assert not paragraph.as_slide()


def test_children_outlive_the_document(odt_path):
    def collect():
        document = pyodr.open(str(odt_path)).as_document_file().document()
        return list(document.root_element())

    # the document is only reachable through the elements by now
    assert [child.type() for child in collect()]


def test_text_root(odt_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    root = document.root_element().as_text_root()
    assert root
    layout = root.page_layout()
    assert isinstance(layout, pyodr.PageLayout)


def test_document_filesystem(odt_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    filesystem = document.as_filesystem()
    assert filesystem.is_file("/content.xml")


def test_list_markers(odt_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()

    lists = [
        child
        for child in document.root_element().children()
        if child.type() == pyodr.ElementType.list
    ]
    assert len(lists) == 2

    bullets, numbers = (element.as_list() for element in lists)
    assert bullets.list_type() == pyodr.ListType.unordered
    assert numbers.list_type() == pyodr.ListType.ordered

    def items(element):
        return [child.as_list_item() for child in element.children()]

    assert [item.marker() for item in items(lists[0])] == ["•"]
    assert [item.number() for item in items(lists[0])] == [None]

    assert [item.marker() for item in items(lists[1])] == ["1.", "2."]
    assert [item.number() for item in items(lists[1])] == [1, 2]


def test_save_to_memory_round_trips(odt_path, tmp_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    assert document.is_savable()

    saved = document.save_to_memory()
    assert isinstance(saved, bytes)
    assert saved[:2] == b"PK"

    path = tmp_path / "from_memory.odt"
    path.write_bytes(saved)
    reloaded = pyodr.open(str(path)).as_document_file().document()
    assert walk_text(reloaded.root_element()) == walk_text(document.root_element())


def test_save_to_memory_carries_an_edit(odt_path, tmp_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()

    run = document.root_element().first_child().first_child()
    diff = (
        '{"version":2,"ops":[{"op":"setText","id":%d,'
        '"text":"edited in python"}]}' % run.identifier()
    )
    document.edit(diff)

    path = tmp_path / "edited.odt"
    path.write_bytes(document.save_to_memory())
    reloaded = pyodr.open(str(path)).as_document_file().document()

    assert "edited in python" in walk_text(reloaded.root_element())


def test_structural_edits_build_a_document_in_process(odt_path, tmp_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    body = document.root_element().first_child()
    first = body.as_paragraph()
    run = first.first_child().as_text()

    document.insert_text_before(run, "before ")
    document.insert_text_after(run, " after")

    added = document.insert_paragraph_after(first)
    document.append_text(added, "a new paragraph")

    path = tmp_path / "structural.odt"
    path.write_bytes(document.save_to_memory())
    text = walk_text(pyodr.open(str(path)).as_document_file().document().root_element())

    assert "before Hello from pyodr! after" in text
    assert "a new paragraph" in text


def test_remove_takes_the_element_out(odt_path, tmp_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    run = document.root_element().first_child().first_child().as_text()

    document.remove(run)

    path = tmp_path / "removed.odt"
    path.write_bytes(document.save_to_memory())
    text = walk_text(pyodr.open(str(path)).as_document_file().document().root_element())

    assert "Hello from pyodr!" not in text
    assert "Second paragraph" in text


def test_split_and_merge_are_inverse(odt_path, tmp_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    first = document.root_element().first_child().as_paragraph()
    run = first.first_child().as_text()

    document.split_paragraph(first, run)
    document.merge_paragraph_with_next(first)

    path = tmp_path / "split.odt"
    path.write_bytes(document.save_to_memory())
    text = walk_text(pyodr.open(str(path)).as_document_file().document().root_element())

    assert "Hello from pyodr!" in text


def first_text(element):
    if element.type() == pyodr.ElementType.text:
        return element.as_text()
    for child in element.children():
        found = first_text(child)
        if found is not None:
            return found
    return None


def test_set_style_marks_a_run(odt_path, tmp_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()
    run = first_text(document.root_element())

    style = pyodr.TextStyle()
    style.font_weight = pyodr.FontWeight.bold
    style.font_size = pyodr.Measure("14pt")
    style.background_color = pyodr.Color(0xFF, 0xFF, 0x00)
    run.set_style(style)

    path = tmp_path / "styled.odt"
    path.write_bytes(document.save_to_memory())
    reloaded = pyodr.open(str(path)).as_document_file().document()
    styled = first_text(reloaded.root_element()).style()

    assert styled.font_weight == pyodr.FontWeight.bold
    assert styled.font_size == pyodr.Measure("14pt")
    assert styled.background_color.rgb() == 0xFFFF00
    assert styled.font_style is None


def test_set_cell_style_fills_a_cell(ods_path, tmp_path):
    document = pyodr.open(str(ods_path)).as_document_file().document()
    sheet = next(iter(document.root_element().children())).as_sheet()

    cell_style = pyodr.TableCellStyle()
    cell_style.background_color = pyodr.Color(0xFF, 0xFF, 0x00)
    cell_style.horizontal_align = pyodr.HorizontalAlign.center
    text_style = pyodr.TextStyle()
    text_style.font_weight = pyodr.FontWeight.bold
    sheet.set_cell_style(0, 0, cell_style, text_style)

    path = tmp_path / "styled.ods"
    path.write_bytes(document.save_to_memory())
    reloaded = pyodr.open(str(path)).as_document_file().document()
    reloaded_sheet = next(iter(reloaded.root_element().children())).as_sheet()

    assert reloaded_sheet.cell_style(0, 0).background_color.rgb() == 0xFFFF00
    assert first_text(reloaded_sheet.cell(0, 0)).style().font_weight == (
        pyodr.FontWeight.bold
    )


def test_set_row_and_column_style_reach_past_the_cells(ods_path, tmp_path):
    document = pyodr.open(str(ods_path)).as_document_file().document()
    sheet = next(iter(document.root_element().children())).as_sheet()

    cell_style = pyodr.TableCellStyle()
    cell_style.background_color = pyodr.Color(0xFF, 0xFF, 0x00)
    sheet.set_row_style(40, cell_style, pyodr.TextStyle())
    sheet.set_column_style(30, cell_style, pyodr.TextStyle())

    path = tmp_path / "styled.ods"
    path.write_bytes(document.save_to_memory())
    reloaded = pyodr.open(str(path)).as_document_file().document()
    reloaded_sheet = next(iter(reloaded.root_element().children())).as_sheet()

    assert reloaded_sheet.cell_style(0, 40).background_color.rgb() == 0xFFFF00
    assert reloaded_sheet.cell_style(30, 90).background_color.rgb() == 0xFFFF00
    assert reloaded_sheet.cell_style(0, 41).background_color is None


def test_locale_is_the_language_of_the_default_style(ods_path, tmp_path):
    assert pyodr.open(str(ods_path)).as_document_file().document().locale() is None

    path = tmp_path / "german.fods"
    path.write_text(
        '<office:document xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"'
        ' xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"'
        ' xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"'
        ' office:mimetype="application/vnd.oasis.opendocument.spreadsheet">'
        '<office:styles><style:default-style style:family="table-cell">'
        '<style:text-properties fo:language="de" fo:country="DE"/>'
        "</style:default-style></office:styles>"
        "<office:body><office:spreadsheet/></office:body></office:document>"
    )

    assert pyodr.open(str(path)).as_document_file().document().locale() == "de-DE"


def test_set_cell_style_refuses_what_no_engine_writes(ods_path):
    document = pyodr.open(str(ods_path)).as_document_file().document()
    sheet = next(iter(document.root_element().children())).as_sheet()

    cell_style = pyodr.TableCellStyle()
    cell_style.wrap_text = True
    with pytest.raises(pyodr.UnsupportedOperation):
        sheet.set_cell_style(0, 0, cell_style, pyodr.TextStyle())


def first_paragraph(element):
    if element.type() == pyodr.ElementType.paragraph:
        return element.as_paragraph()
    for child in element.children():
        found = first_paragraph(child)
        if found is not None:
            return found
    return None


def test_set_style_aligns_a_paragraph(odt_path, tmp_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()

    style = pyodr.ParagraphStyle()
    style.text_align = pyodr.TextAlign.center
    first_paragraph(document.root_element()).set_style(style)

    path = tmp_path / "aligned.odt"
    path.write_bytes(document.save_to_memory())
    reloaded = pyodr.open(str(path)).as_document_file().document()

    assert first_paragraph(reloaded.root_element()).style().text_align == (
        pyodr.TextAlign.center
    )


def test_set_paragraph_style_refuses_what_no_engine_writes(odt_path):
    document = pyodr.open(str(odt_path)).as_document_file().document()

    style = pyodr.ParagraphStyle()
    style.line_height = pyodr.Measure("12pt")
    with pytest.raises(pyodr.UnsupportedOperation):
        first_paragraph(document.root_element()).set_style(style)


def test_create_document_makes_every_type_that_states_create():
    for file_type in pyodr.all_file_types():
        if not pyodr.capabilities_by_file_type(file_type).create:
            continue
        document = pyodr.create_document(file_type)
        assert document.file_type() == file_type
        assert document.is_editable()


def test_create_document_refuses_a_type_without_create():
    with pytest.raises(pyodr.UnsupportedFileTypeError):
        pyodr.create_document(pyodr.FileType.office_open_xml_presentation)


def test_a_created_text_document_takes_an_edit(tmp_path):
    document = pyodr.create_document(pyodr.FileType.office_open_xml_document)
    paragraph = document.root_element().first_child().as_paragraph()

    document.append_text(paragraph, "written in python")

    path = tmp_path / "created.docx"
    path.write_bytes(document.save_to_memory())
    text = walk_text(pyodr.open(str(path)).as_document_file().document().root_element())

    assert text == ["written in python"]


def test_a_created_sheet_takes_an_edit(tmp_path):
    document = pyodr.create_document(pyodr.FileType.opendocument_spreadsheet)
    sheet = document.root_element().first_child().as_sheet()

    assert sheet.name() == "Sheet1"

    document.edit(
        '{"version":2,"ops":[{"op":"setCell","sheet":0,"column":1,"row":2,'
        '"value":{"type":"number","number":12.5,"text":"12.5"}}]}'
    )

    path = tmp_path / "created.ods"
    path.write_bytes(document.save_to_memory())
    reloaded = pyodr.open(str(path)).as_document_file().document()
    cell = reloaded.root_element().first_child().as_sheet().cell(1, 2)

    assert cell.value_type() == pyodr.ValueType.float_number
    assert walk_text(cell) == ["12.5"]
