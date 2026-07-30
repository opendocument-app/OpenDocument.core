package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class DocumentTest {
  @TempDir Path tempDir;

  private Document openDocument() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    return Odr.open(odt.toString()).asDocumentFile().document();
  }

  private static List<String> walkText(Element element) {
    List<String> parts = new ArrayList<>();
    if (element.type() == ElementType.TEXT) {
      parts.add(element.asText().content());
    }
    for (Element child : element.children()) {
      parts.addAll(walkText(child));
    }
    return parts;
  }

  @Test
  void elementTree() throws IOException {
    Document document = openDocument();
    assertEquals(DocumentType.TEXT, document.documentType());
    assertEquals(FileType.OPENDOCUMENT_TEXT, document.fileType());

    Element root = document.rootElement();
    assertEquals(ElementType.ROOT, root.type());

    List<Element> paragraphs =
        root.children().stream().filter(child -> child.type() == ElementType.PARAGRAPH).toList();
    assertEquals(2, paragraphs.size());
    assertNotNull(paragraphs.get(0).asParagraph());

    List<String> text = walkText(root);
    assertTrue(text.contains(TestFiles.ODT_FIRST_PARAGRAPH));
    // Exercises non-BMP characters across the JNI string conversion.
    assertTrue(text.contains(TestFiles.ODT_SECOND_PARAGRAPH));
  }

  @Test
  void elementNavigation() throws IOException {
    Document document = openDocument();
    Element root = document.rootElement();

    Element first = root.firstChild();
    assertNotNull(first);
    assertTrue(first.parent().isSame(root));
    Element second = first.nextSibling();
    assertNotNull(second);
    assertTrue(second.previousSibling().isSame(first));
  }

  @Test
  void textRoot() throws IOException {
    Document document = openDocument();
    TextRoot root = document.rootElement().asTextRoot();
    assertNotNull(root);
    assertNotNull(root.pageLayout());
  }

  @Test
  void documentFilesystem() throws IOException {
    Document document = openDocument();
    Filesystem filesystem = document.asFilesystem();
    assertTrue(filesystem.isFile("/content.xml"));
  }

  @Test
  void documentPath() throws IOException {
    Document document = openDocument();
    Element first = document.rootElement().firstChild();
    DocumentPath path = first.documentPath();
    assertNotNull(path.toString());
    assertEquals(path, first.documentPath());

    // join() and navigatePath() take another wrapper's handle as an argument
    DocumentPath rejoined = path.parent().join(path);
    assertTrue(path.parent().empty());
    assertEquals(path, rejoined);
    assertTrue(document.rootElement().navigatePath(rejoined).isSame(first));
  }

  @Test
  void editAppliesADiff() throws IOException {
    Document document = openDocument();
    assertTrue(document.isEditable());

    Element paragraph = document.rootElement().firstChild();
    DocumentPath text = paragraph.firstChild().documentPath();

    Html.edit(document, "{\"modifiedText\":{\"" + text + "\":\"edited by the diff\"}}");

    assertTrue(walkText(document.rootElement()).contains("edited by the diff"));
  }
}
