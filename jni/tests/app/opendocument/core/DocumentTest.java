package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.file.Files;
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
    assertEquals(4, paragraphs.size());
    assertNotNull(paragraphs.get(0).asParagraph());

    // Every text node of the document, in order — the spans included.
    assertEquals(TestFiles.ODT_TEXT, walkText(root));
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
    long text = paragraph.firstChild().identifier();

    document.edit("{\"version\":2,\"ops\":[{\"op\":\"setText\",\"id\":"
        + text + ",\"text\":\"edited by the diff\"}]}");

    assertTrue(walkText(document.rootElement()).contains("edited by the diff"));
  }

  @Test
  void saveToMemoryRoundTripsAnEdit() throws IOException {
    Document document = openDocument();

    Element paragraph = document.rootElement().firstChild();
    long text = paragraph.firstChild().identifier();
    document.edit("{\"version\":2,\"ops\":[{\"op\":\"setText\",\"id\":"
        + text + ",\"text\":\"saved to memory\"}]}");

    byte[] saved = document.saveToMemory();
    assertTrue(saved.length > 0);

    Path reloadedPath = tempDir.resolve("from-memory.odt");
    Files.write(reloadedPath, saved);
    Document reloaded = Odr.open(reloadedPath.toString()).asDocumentFile().document();

    assertTrue(walkText(reloaded.rootElement()).contains("saved to memory"));
  }

  @Test
  void elementByIdResolvesWhatIdentifierHandedOut() throws IOException {
    Document document = openDocument();
    Element run = document.rootElement().firstChild().firstChild();

    Element found = document.elementById(run.identifier());

    assertNotNull(found);
    assertEquals(run.asText().content(), found.asText().content());
    assertNull(document.elementById(999999));
  }

  @Test
  void structuralEditsBuildADocumentInProcess() throws IOException {
    Document document = openDocument();
    Paragraph first = document.rootElement().firstChild().asParagraph();
    Text run = first.firstChild().asText();

    document.insertTextBefore(run, "before ");
    document.insertTextAfter(run, " after");

    Paragraph added = document.insertParagraphAfter(first);
    document.appendText(added, "a new paragraph");

    List<String> text = walkText(document.rootElement());
    assertTrue(text.contains("before "));
    assertTrue(text.contains(" after"));
    assertTrue(text.contains("a new paragraph"));
  }

  @Test
  void removeTakesTheElementOut() throws IOException {
    Document document = openDocument();
    Element run = document.rootElement().firstChild().firstChild();

    document.remove(run);

    // The fixture repeats its runs, so what proves the removal is the count.
    List<String> text = walkText(document.rootElement());
    assertEquals(TestFiles.ODT_TEXT.size() - 1, text.size());
    assertEquals(TestFiles.ODT_TEXT.subList(1, TestFiles.ODT_TEXT.size()), text);
  }

  @Test
  void splitAndMergeAreInverse() throws IOException {
    Document document = openDocument();
    Paragraph first = document.rootElement().firstChild().asParagraph();
    Text run = first.firstChild().asText();

    document.splitParagraph(first, run);
    document.mergeParagraphWithNext(first);

    byte[] saved = document.saveToMemory();
    Path path = tempDir.resolve("split.odt");
    Files.write(path, saved);
    Document reloaded = Odr.open(path.toString()).asDocumentFile().document();

    assertTrue(walkText(reloaded.rootElement()).contains(TestFiles.ODT_TEXT.get(0)));
  }
}
