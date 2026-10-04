package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
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
  void createDocumentMakesEveryTypeThatStatesCreate() {
    for (FileType type : Odr.allFileTypes()) {
      if (!Odr.capabilitiesByFileType(type).create) {
        continue;
      }
      Document document = Odr.createDocument(type);
      assertEquals(type, document.fileType());
      assertTrue(document.isEditable());
    }
  }

  @Test
  void createDocumentRefusesATypeWithoutCreate() {
    assertThrows(
        OdrException.UnsupportedFileType.class,
        () -> Odr.createDocument(FileType.OFFICE_OPEN_XML_PRESENTATION));
  }

  @Test
  void aCreatedDocumentTakesAnEditThroughASave() throws IOException {
    Document document = Odr.createDocument(FileType.OPENDOCUMENT_TEXT);
    Paragraph paragraph = document.rootElement().firstChild().asParagraph();

    document.appendText(paragraph, "written in java");

    Path path = tempDir.resolve("created.odt");
    Files.write(path, document.saveToMemory());
    Document saved = Odr.open(path.toString()).asDocumentFile().document();
    assertEquals(List.of("written in java"), walkText(saved.rootElement()));
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

  private static Text firstText(Element element) {
    if (element.type() == ElementType.TEXT) {
      return element.asText();
    }
    for (Element child : element.children()) {
      Text found = firstText(child);
      if (found != null) {
        return found;
      }
    }
    return null;
  }

  @Test
  void setStyleMarksARun() throws IOException {
    Document document = openDocument();
    Text run = firstText(document.rootElement());

    TextStyle style = new TextStyle();
    style.fontWeight = FontWeight.BOLD;
    style.fontSize = new Measure(14, "pt");
    style.backgroundColor = new Color(255, 255, 0);
    run.setStyle(style);

    Path path = tempDir.resolve("styled.odt");
    Files.write(path, document.saveToMemory());
    Document reloaded = Odr.open(path.toString()).asDocumentFile().document();
    TextStyle styled = firstText(reloaded.rootElement()).style();

    assertEquals(FontWeight.BOLD, styled.fontWeight);
    assertEquals(14.0, styled.fontSize.magnitude);
    assertEquals("pt", styled.fontSize.unit);
    assertEquals(new Color(255, 255, 0), styled.backgroundColor);
    assertNull(styled.fontStyle);
  }

  private static Paragraph firstParagraph(Element element) {
    if (element.type() == ElementType.PARAGRAPH) {
      return element.asParagraph();
    }
    for (Element child : element.children()) {
      Paragraph found = firstParagraph(child);
      if (found != null) {
        return found;
      }
    }
    return null;
  }

  @Test
  void setStyleAlignsAParagraph() throws IOException {
    Document document = openDocument();

    ParagraphStyle style = new ParagraphStyle();
    style.textAlign = TextAlign.CENTER;
    firstParagraph(document.rootElement()).setStyle(style);

    Path path = tempDir.resolve("aligned.odt");
    Files.write(path, document.saveToMemory());
    Document reloaded = Odr.open(path.toString()).asDocumentFile().document();

    assertEquals(TextAlign.CENTER, firstParagraph(reloaded.rootElement()).style().textAlign);
  }

  @Test
  void setParagraphStyleRefusesWhatNoEngineWrites() throws IOException {
    Document document = openDocument();
    Paragraph paragraph = firstParagraph(document.rootElement());

    ParagraphStyle style = new ParagraphStyle();
    style.textAlign = TextAlign.CENTER;
    style.lineHeight = new Measure(12, "pt");
    assertThrows(OdrException.UnsupportedOperation.class, () -> paragraph.setStyle(style));
  }

  @Test
  void setCellStyleFillsACell() throws IOException {
    Path ods = TestFiles.odsFile(tempDir);
    Document document = Odr.open(ods.toString()).asDocumentFile().document();
    Sheet sheet = document.rootElement().firstChild().asSheet();

    TableCellStyle cellStyle = new TableCellStyle();
    cellStyle.backgroundColor = new Color(255, 255, 0);
    cellStyle.horizontalAlign = HorizontalAlign.CENTER;
    TextStyle textStyle = new TextStyle();
    textStyle.fontWeight = FontWeight.BOLD;
    sheet.setCellStyle(0, 0, cellStyle, textStyle);

    Path path = tempDir.resolve("styled.ods");
    Files.write(path, document.saveToMemory());
    Document reloaded = Odr.open(path.toString()).asDocumentFile().document();
    Sheet reloadedSheet = reloaded.rootElement().firstChild().asSheet();

    assertEquals(new Color(255, 255, 0), reloadedSheet.cellStyle(0, 0).backgroundColor);
    assertEquals(FontWeight.BOLD, firstText(reloadedSheet.cell(0, 0)).style().fontWeight);
  }

  @Test
  void localeIsTheLanguageOfTheDefaultStyle() throws IOException {
    Path ods = TestFiles.odsFile(tempDir);
    assertEquals("en-US", Odr.open(ods.toString()).asDocumentFile().document().locale());
  }

  @Test
  void setRowAndColumnStyleReachPastTheCells() throws IOException {
    Path ods = TestFiles.odsFile(tempDir);
    Document document = Odr.open(ods.toString()).asDocumentFile().document();
    Sheet sheet = document.rootElement().firstChild().asSheet();

    TableCellStyle cellStyle = new TableCellStyle();
    cellStyle.backgroundColor = new Color(255, 255, 0);
    sheet.setRowStyle(40, cellStyle, new TextStyle());
    sheet.setColumnStyle(30, cellStyle, new TextStyle());

    Path path = tempDir.resolve("styled.ods");
    Files.write(path, document.saveToMemory());
    Document reloaded = Odr.open(path.toString()).asDocumentFile().document();
    Sheet reloadedSheet = reloaded.rootElement().firstChild().asSheet();

    assertEquals(new Color(255, 255, 0), reloadedSheet.cellStyle(0, 40).backgroundColor);
    assertEquals(new Color(255, 255, 0), reloadedSheet.cellStyle(30, 90).backgroundColor);
  }

  @Test
  void insertAndDeleteRowsMoveTheCells() throws IOException {
    Path ods = TestFiles.odsFile(tempDir);
    Document document = Odr.open(ods.toString()).asDocumentFile().document();
    Sheet sheet = document.rootElement().firstChild().asSheet();
    String text = firstText(sheet.cell(0, 0)).content();

    sheet.insertRows(0, 2);

    Path path = tempDir.resolve("rows.ods");
    Files.write(path, document.saveToMemory());
    Document reloaded = Odr.open(path.toString()).asDocumentFile().document();
    Sheet reloadedSheet = reloaded.rootElement().firstChild().asSheet();
    assertEquals(text, firstText(reloadedSheet.cell(0, 2)).content());

    reloadedSheet.deleteRows(0, 2);
    assertEquals(text, firstText(reloadedSheet.cell(0, 0)).content());
  }

  @Test
  void insertAndDeleteColumnsMoveTheCells() throws IOException {
    Path ods = TestFiles.odsFile(tempDir);
    Document document = Odr.open(ods.toString()).asDocumentFile().document();
    Sheet sheet = document.rootElement().firstChild().asSheet();
    String text = firstText(sheet.cell(0, 0)).content();

    sheet.insertColumns(0, 2);

    Path path = tempDir.resolve("columns.ods");
    Files.write(path, document.saveToMemory());
    Document reloaded = Odr.open(path.toString()).asDocumentFile().document();
    Sheet reloadedSheet = reloaded.rootElement().firstChild().asSheet();
    assertEquals(text, firstText(reloadedSheet.cell(2, 0)).content());

    reloadedSheet.deleteColumns(0, 2);
    assertEquals(text, firstText(reloadedSheet.cell(0, 0)).content());
  }

  @Test
  void setCellStyleRefusesWhatNoEngineWrites() throws IOException {
    Path ods = TestFiles.odsFile(tempDir);
    Document document = Odr.open(ods.toString()).asDocumentFile().document();
    Sheet sheet = document.rootElement().firstChild().asSheet();

    TableCellStyle cellStyle = new TableCellStyle();
    cellStyle.wrapText = true;
    assertThrows(
        OdrException.UnsupportedOperation.class,
        () -> sheet.setCellStyle(0, 0, cellStyle, new TextStyle()));
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

  private static final String FLAT_SPREADSHEET =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          + "<office:document"
          + " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
          + " xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\""
          + " xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\""
          + " office:version=\"1.3\""
          + " office:mimetype=\"application/vnd.oasis.opendocument.spreadsheet\">"
          + "<office:body><office:spreadsheet><table:table table:name=\"s\">"
          + "<table:table-row>"
          + "<table:table-cell office:value-type=\"float\" office:value=\"1\">"
          + "<text:p>1</text:p></table:table-cell>"
          + "<table:table-cell table:formula=\"of:=[.A1]*2\""
          + " office:value-type=\"float\" office:value=\"2\"><text:p>2</text:p>"
          + "</table:table-cell>"
          + "<table:table-cell table:formula=\"of:=[.C1]+1\"/>"
          + "</table:table-row></table:table>"
          + "</office:spreadsheet></office:body></office:document>";

  @Test
  void recalculateComputesTheStaleFormulas() throws IOException {
    Path path = tempDir.resolve("formulas.fods");
    Files.writeString(path, FLAT_SPREADSHEET);
    Document document = Odr.open(path.toString()).asDocumentFile().document();

    document.edit(
        "{\"version\":2,\"ops\":[{\"op\":\"setCell\",\"sheet\":0,\"column\":0,\"row\":0,"
            + "\"value\":{\"type\":\"number\",\"number\":5,\"text\":\"5\"}}]}");
    Recalculation result = document.recalculate();

    assertEquals(List.of(new SheetPosition(0, 1, 0)), result.changed());
    assertEquals(List.of(new SheetPosition(0, 2, 0)), result.circular());
    assertTrue(result.unevaluated().isEmpty());
    assertTrue(document.recalculate().changed().isEmpty());
  }
}
