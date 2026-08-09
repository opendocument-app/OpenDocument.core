import pyodr


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
