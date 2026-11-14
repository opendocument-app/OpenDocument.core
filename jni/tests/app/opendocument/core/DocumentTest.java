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
  void closingDocumentInvalidatesDerivedElements() throws IOException {
    Document document = openDocument();
    try (Element root = document.rootElement(); TextRoot typed = root.asTextRoot()) {
      document.close();
      assertThrows(IllegalStateException.class, root::type);
      assertThrows(IllegalStateException.class, typed::pageLayout);
    }
  }

  @Test
  void missingRootReturnsNull() throws IOException {
    Path path = TestFiles.missingRootFile(tempDir);
    try (Document document = Odr.open(path.toString()).asDocumentFile().document()) {
      assertEquals(DocumentType.TEXT, document.documentType());
      assertNull(document.rootElement());
    }
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
  void aParagraphOutsideANumberedOutlineHasNoMarker() throws IOException {
    Document document = openDocument();

    // The fixture has no heading, so no marker.
    List<Element> paragraphs =
        document.rootElement().children().stream()
            .filter(child -> child.type() == ElementType.PARAGRAPH)
            .toList();
    assertEquals(4, paragraphs.size());
    for (Element paragraph : paragraphs) {
      assertEquals("", paragraph.asParagraph().marker());
    }
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
        () -> Odr.createDocument(FileType.LEGACY_WORD_DOCUMENT));
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
  void insertSheetNamesAndPlacesIt() throws IOException {
    for (FileType type :
        List.of(FileType.OPENDOCUMENT_SPREADSHEET, FileType.OFFICE_OPEN_XML_WORKBOOK)) {
      Document document = Odr.createDocument(type);

      assertEquals("Front", document.insertSheet(0, "Front").name());
      assertEquals("Sheet3", document.insertSheet(2).name());
      assertThrows(OdrException.class, () -> document.insertSheet(0, "front"));

      Path path =
          tempDir.resolve(
              "sheets." + (type == FileType.OPENDOCUMENT_SPREADSHEET ? "ods" : "xlsx"));
      Files.write(path, document.saveToMemory());
      Document saved = Odr.open(path.toString()).asDocumentFile().document();
      List<String> names = new ArrayList<>();
      for (Element sheet : saved.rootElement().children()) {
        names.add(sheet.asSheet().name());
      }
      assertEquals(List.of("Front", "Sheet1", "Sheet3"), names);
    }
  }

  @Test
  void aSlideFromALayoutAndATextBoxSurviveASave() throws IOException {
    for (FileType type :
        List.of(FileType.OPENDOCUMENT_PRESENTATION, FileType.OFFICE_OPEN_XML_PRESENTATION)) {
      Document document = Odr.createDocument(type);

      List<SlideLayout> layouts = document.slideLayouts();
      List<String> names = new ArrayList<>();
      for (SlideLayout layout : layouts) {
        names.add(layout.name);
      }
      assertEquals(
          List.of(
              "Title Slide",
              "Title and Content",
              "Section Header",
              "Two Content",
              "Title Only",
              "Blank"),
          names);
      assertEquals(SlidePlaceholderKind.TITLE, layouts.get(1).placeholders.get(0).kind);
      assertEquals(SlidePlaceholderKind.BODY, layouts.get(1).placeholders.get(1).kind);

      Slide slide = document.insertSlide(1, 1);
      assertEquals(2, slide.children().size());
      document.appendText(firstParagraph(slide.firstChild()), "Agenda");

      Frame box =
          document.insertTextBox(
              slide,
              new Measure(2, "cm"),
              new Measure(12, "cm"),
              new Measure(10, "cm"),
              new Measure(1.5, "cm"));
      document.appendText(firstParagraph(box), "note");
      assertThrows(
          OdrException.class,
          () ->
              document.insertTextBox(
                  slide,
                  new Measure(2, "cm"),
                  new Measure(12, "cm"),
                  new Measure(0, "cm"),
                  new Measure(1, "cm")));
      assertThrows(OdrException.class, () -> document.insertSlide(9, 0));

      Path path =
          tempDir.resolve(
              "slides." + (type == FileType.OPENDOCUMENT_PRESENTATION ? "odp" : "pptx"));
      Files.write(path, document.saveToMemory());
      Document saved = Odr.open(path.toString()).asDocumentFile().document();
      List<Element> slides = saved.rootElement().children();
      assertEquals(2, slides.size());
      assertEquals(List.of("Agenda", "note"), walkText(slides.get(1)));
    }
  }

  @Test
  void aTextDocumentOffersNoSlideLayout() {
    assertTrue(Odr.createDocument(FileType.OPENDOCUMENT_TEXT).slideLayouts().isEmpty());
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
    for (Color invalid : new Color[] {new Color(-1, 0, 0), new Color(0, 256, 0),
        new Color(0, 0, -1), new Color(0, 0, 0, 256)}) {
      style.backgroundColor = invalid;
      assertThrows(OdrException.class, () -> run.setStyle(style));
    }
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
  void aSheetCellStatesItsNumberFormat() throws IOException {
    Path fods = tempDir.resolve("percent.fods");
    Files.writeString(
        fods,
        "<office:document"
            + " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
            + " xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\""
            + " xmlns:number=\"urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0\""
            + " xmlns:table=\"urn:oasis:names:tc:opendocument:xmlns:table:1.0\""
            + " xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\""
            + " office:mimetype=\"application/vnd.oasis.opendocument.spreadsheet\">"
            + "<office:automatic-styles><number:percentage-style style:name=\"N1\">"
            + "<number:number number:decimal-places=\"1\" number:min-integer-digits=\"1\"/>"
            + "<number:text>%</number:text></number:percentage-style>"
            + "<style:style style:name=\"ce1\" style:family=\"table-cell\""
            + " style:data-style-name=\"N1\"/></office:automatic-styles>"
            + "<office:body><office:spreadsheet><table:table table:name=\"s\"><table:table-row>"
            + "<table:table-cell table:style-name=\"ce1\" office:value-type=\"percentage\""
            + " office:value=\"0.25\"><text:p>25.0%</text:p></table:table-cell>"
            + "<table:table-cell office:value-type=\"float\" office:value=\"1\">"
            + "<text:p>1</text:p></table:table-cell>"
            + "</table:table-row></table:table></office:spreadsheet></office:body>"
            + "</office:document>");
    Sheet sheet =
        Odr.open(fods.toString()).asDocumentFile().document().rootElement().firstChild().asSheet();

    assertEquals("0.0%", sheet.cell(0, 0).numberFormat());
    assertNull(sheet.cell(1, 0).numberFormat());
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
  void negativeSheetCoordinatesAreRejected() throws IOException {
    try (DecodedFile file = Odr.open(TestFiles.odsFile(tempDir).toString())) {
      Document document = file.asDocumentFile().document();
      Sheet sheet = document.rootElement().firstChild().asSheet();
      assertThrows(OdrException.class, () -> sheet.cell(-1, 0));
      assertThrows(OdrException.class, () -> sheet.cell(0, -1));
      assertThrows(OdrException.class, () -> sheet.insertRows(0, -1));
      assertThrows(OdrException.class, () -> sheet.deleteColumns(-1, 1));
      assertThrows(IllegalArgumentException.class,
          () -> sheet.content(new TableDimensions(-1, 1)));
      assertThrows(IllegalArgumentException.class,
          () -> sheet.content(new TableDimensions(-1, -1)));
      assertNotNull(sheet.content(null));
    }
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

  private static final String FILLED_DRAWING =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          + "<office:document"
          + " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
          + " xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\""
          + " xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\""
          + " xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\""
          + " office:version=\"1.3\""
          + " office:mimetype=\"application/vnd.oasis.opendocument.graphics\">"
          + "<office:styles>"
          + "<draw:gradient draw:name=\"G\" draw:style=\"radial\""
          + " draw:start-color=\"#ff0000\" draw:end-color=\"#0000ff\""
          + " draw:angle=\"45deg\" draw:border=\"20%\" draw:cx=\"25%\" draw:cy=\"75%\"/>"
          + "<draw:hatch draw:name=\"H\" draw:style=\"triple\" draw:color=\"#3465a4\""
          + " draw:distance=\"0.2cm\" draw:rotation=\"450\"/>"
          + "<style:style style:name=\"g\" style:family=\"graphic\">"
          + "<style:graphic-properties draw:fill=\"gradient\" draw:fill-gradient-name=\"G\"/>"
          + "</style:style>"
          + "<style:style style:name=\"h\" style:family=\"graphic\">"
          + "<style:graphic-properties draw:fill=\"hatch\" draw:fill-hatch-name=\"H\"/>"
          + "</style:style>"
          + "</office:styles>"
          + "<office:body><office:drawing><draw:page>"
          + "<draw:rect draw:style-name=\"g\" svg:width=\"4cm\" svg:height=\"2cm\"/>"
          + "<draw:rect draw:style-name=\"h\" svg:width=\"4cm\" svg:height=\"2cm\"/>"
          + "</draw:page></office:drawing></office:body></office:document>";

  @Test
  void frameStyleCarriesGradientAndHatchFills() throws IOException {
    Path path = tempDir.resolve("filled.fodg");
    Files.writeString(path, FILLED_DRAWING);
    Document document = Odr.open(path.toString()).asDocumentFile().document();
    List<Element> frames = document.rootElement().firstChild().children();

    GraphicStyle gradientStyle = frames.get(0).asFrame().style();
    assertNull(gradientStyle.fillHatch);
    GradientFill gradient = gradientStyle.fillGradient;
    assertNotNull(gradient);
    assertEquals(GradientStyle.RADIAL, gradient.style);
    assertEquals(0xff0000, gradient.startColor.rgb());
    assertEquals(0x0000ff, gradient.endColor.rgb());
    assertEquals(45, gradient.angle, 1e-9);
    assertEquals(0.2, gradient.border, 1e-9);
    assertEquals(0.25, gradient.centerX, 1e-9);
    assertEquals(0.75, gradient.centerY, 1e-9);
    GradientFill gradientAgain = frames.get(0).asFrame().style().fillGradient;
    assertEquals(gradient, gradientAgain);
    assertEquals(gradient.hashCode(), gradientAgain.hashCode());

    GraphicStyle hatchStyle = frames.get(1).asFrame().style();
    assertNull(hatchStyle.fillGradient);
    HatchFill hatch = hatchStyle.fillHatch;
    assertNotNull(hatch);
    assertEquals(HatchStyle.TRIPLE, hatch.style);
    assertEquals(0x3465a4, hatch.color.rgb());
    assertEquals(new Measure(0.2, "cm"), hatch.distance);
    assertEquals(45, hatch.rotation, 1e-9);
    HatchFill hatchAgain = frames.get(1).asFrame().style().fillHatch;
    assertEquals(hatch, hatchAgain);
    assertEquals(hatch.hashCode(), hatchAgain.hashCode());
  }

  private static final String BITMAP_PAGE_DRAWING =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          + "<office:document"
          + " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
          + " xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\""
          + " xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\""
          + " office:version=\"1.3\""
          + " office:mimetype=\"application/vnd.oasis.opendocument.graphics\">"
          + "<office:styles><draw:fill-image draw:name=\"Red\"><office:binary-data>"
          + "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQDJ/pLvAAAAAElFTkSuQmCC"
          + "</office:binary-data></draw:fill-image></office:styles>"
          + "<office:automatic-styles>"
          + "<style:style style:name=\"Mdp\" style:family=\"drawing-page\">"
          + "<style:drawing-page-properties draw:fill=\"bitmap\""
          + " draw:fill-image-name=\"Red\" style:repeat=\"no-repeat\"/>"
          + "</style:style></office:automatic-styles>"
          + "<office:master-styles>"
          + "<style:master-page style:name=\"M\" draw:style-name=\"Mdp\"/>"
          + "</office:master-styles>"
          + "<office:body><office:drawing><draw:page draw:master-page-name=\"M\"/>"
          + "</office:drawing></office:body></office:document>";

  @Test
  void pageBackgroundCarriesItsMasterPageBitmap() throws IOException {
    Path path = tempDir.resolve("bitmap.fodg");
    Files.writeString(path, BITMAP_PAGE_DRAWING);
    Document document = Odr.open(path.toString()).asDocumentFile().document();

    GraphicStyle background = document.rootElement().firstChild().asPage().background();
    assertNull(background.fillGradient);
    BitmapFill bitmap = background.fillBitmap;
    assertNotNull(bitmap);
    assertEquals(BitmapRepeat.NO_REPEAT, bitmap.repeat);
    assertEquals(69, bitmap.file.size());
    assertEquals("", bitmap.href);
  }

  private static final String FONTWORK_DRAWING =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          + "<office:document"
          + " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
          + " xmlns:text=\"urn:oasis:names:tc:opendocument:xmlns:text:1.0\""
          + " xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\""
          + " xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\""
          + " office:version=\"1.3\""
          + " office:mimetype=\"application/vnd.oasis.opendocument.graphics\">"
          + "<office:body><office:drawing><draw:page>"
          + "<draw:custom-shape svg:width=\"8cm\" svg:height=\"2cm\"><text:p>Fontwork</text:p><draw:enhanced-geometry svg:viewBox=\"0 0 21600 21600\" draw:enhanced-path=\"M 0 0 L 21600 0 N M 0 21600 L 21600 21600 N\" draw:text-path=\"true\" draw:text-path-mode=\"shape\"/></draw:custom-shape>"
          + "<draw:custom-shape svg:width=\"8cm\" svg:height=\"2cm\"><draw:enhanced-geometry svg:viewBox=\"0 0 21600 21600\" draw:enhanced-path=\"M 0 0 L 21600 0 L 21600 21600 Z N\"/></draw:custom-shape>"
          + "</draw:page></office:drawing></office:body></office:document>";

  @Test
  void frameTextPathIsSetForFontworkOnly() throws IOException {
    Path path = tempDir.resolve("fontwork.fodg");
    Files.writeString(path, FONTWORK_DRAWING);
    Document document = Odr.open(path.toString()).asDocumentFile().document();
    List<Element> frames = document.rootElement().firstChild().children();

    assertEquals(TextPathMode.SHAPE, frames.get(0).asFrame().textPath());
    assertNull(frames.get(1).asFrame().textPath());
  }

  private static final String SHADOWED_DRAWING =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          + "<office:document"
          + " xmlns:office=\"urn:oasis:names:tc:opendocument:xmlns:office:1.0\""
          + " xmlns:style=\"urn:oasis:names:tc:opendocument:xmlns:style:1.0\""
          + " xmlns:draw=\"urn:oasis:names:tc:opendocument:xmlns:drawing:1.0\""
          + " xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\""
          + " xmlns:loext=\"urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0\""
          + " office:version=\"1.3\""
          + " office:mimetype=\"application/vnd.oasis.opendocument.graphics\">"
          + "<office:styles>"
          + "<style:style style:name=\"s\" style:family=\"graphic\">"
          + "<style:graphic-properties draw:shadow=\"visible\""
          + " draw:shadow-offset-x=\"0.3cm\" draw:shadow-offset-y=\"-0.1cm\""
          + " draw:shadow-color=\"#f49100\" draw:shadow-opacity=\"40%\""
          + " loext:shadow-blur=\"0.05cm\"/>"
          + "</style:style>"
          + "<style:style style:name=\"n\" style:family=\"graphic\">"
          + "<style:graphic-properties draw:fill=\"none\"/>"
          + "</style:style>"
          + "</office:styles>"
          + "<office:body><office:drawing><draw:page>"
          + "<draw:rect draw:style-name=\"s\" svg:width=\"4cm\" svg:height=\"2cm\"/>"
          + "<draw:rect draw:style-name=\"n\" svg:width=\"4cm\" svg:height=\"2cm\"/>"
          + "</draw:page></office:drawing></office:body></office:document>";

  @Test
  void frameStyleCarriesItsShadow() throws IOException {
    Path path = tempDir.resolve("shadowed.fodg");
    Files.writeString(path, SHADOWED_DRAWING);
    Document document = Odr.open(path.toString()).asDocumentFile().document();
    List<Element> frames = document.rootElement().firstChild().children();

    Shadow shadow = frames.get(0).asFrame().style().shadow;
    assertNotNull(shadow);
    assertEquals(new Measure(0.3, "cm"), shadow.offsetX);
    assertEquals(new Measure(-0.1, "cm"), shadow.offsetY);
    assertEquals(0xf49100, shadow.color.rgb());
    assertEquals(102, shadow.color.alpha);
    assertEquals(new Measure(0.05, "cm"), shadow.blur);
    Shadow shadowAgain = frames.get(0).asFrame().style().shadow;
    assertEquals(shadow, shadowAgain);
    assertEquals(shadow.hashCode(), shadowAgain.hashCode());

    assertNull(frames.get(1).asFrame().style().shadow);
  }
}
