package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class HtmlTest {
  @TempDir Path tempDir;

  private Html translateOffline(Path input) throws IOException {
    Path cache = Files.createDirectories(tempDir.resolve("cache"));
    Path output = Files.createDirectories(tempDir.resolve("output"));
    DecodedFile file = Odr.open(input.toString());
    HtmlService service = Html.translate(file, cache.toString(), new HtmlConfig());
    return service.bringOffline(output.toString());
  }

  private String renderOdt(HtmlConfig config) throws IOException {
    Path cache = Files.createTempDirectory(tempDir, "render");
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlService service = Html.translate(file, cache.toString(), config);
    return service.listViews().get(0).writeHtml().html;
  }

  @Test
  void htmlConfigDefaults() {
    HtmlConfig config = new HtmlConfig();
    assertTrue(config.embedImages);
    assertTrue(!config.editable);
    assertEquals(HtmlTableGridlines.SOFT, config.spreadsheetGridlines);
    assertEquals(HtmlViewportMode.AUTOMATIC, config.viewportMode);
    assertNull(config.spreadsheetViewportMode);
    assertNull(config.viewportContent);
    assertNull(config.viewportWidth);
    assertNull(config.initialZoom);
  }

  @Test
  void viewportConfigRoundTrips() throws IOException {
    HtmlConfig config = new HtmlConfig();
    config.viewportMode = HtmlViewportMode.FIT_WIDTH;
    config.spreadsheetViewportMode = HtmlViewportMode.ACTUAL_SIZE;
    config.viewportContent = "width=420";
    config.viewportWidth = 420;
    config.initialZoom = 1.5;

    Path cache = Files.createDirectories(tempDir.resolve("cache"));
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlConfig readBack = Html.translate(file, cache.toString(), config).config();

    assertEquals(HtmlViewportMode.FIT_WIDTH, readBack.viewportMode);
    assertEquals(HtmlViewportMode.ACTUAL_SIZE, readBack.spreadsheetViewportMode);
    assertEquals("width=420", readBack.viewportContent);
    assertEquals(Integer.valueOf(420), readBack.viewportWidth);
    assertEquals(Double.valueOf(1.5), readBack.initialZoom);
  }

  /** The C++ suite covers where the floor lands; this only proves it crosses JNI. */
  @Test
  void minContentMarginReachesTheHtml() throws IOException {
    assertNull(new HtmlConfig().minContentMargin.top);
    assertTrue(!renderOdt(new HtmlConfig()).contains(":root{--odr-min-margin"));

    // the field is the host's to null, and reading one is not a crash
    HtmlConfig cleared = new HtmlConfig();
    cleared.minContentMargin = null;
    assertTrue(!renderOdt(cleared).contains(":root{--odr-min-margin"));

    HtmlConfig config = new HtmlConfig();
    config.minContentMargin =
        new DirectionalMeasure(null, new Measure(12, "px"), new Measure(1, "cm"), null);
    assertTrue(
        renderOdt(config).contains(":root{--odr-min-margin-top:12px;--odr-min-margin-left:1cm;}"));

    Path cache = Files.createDirectories(tempDir.resolve("margin"));
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlConfig readBack = Html.translate(file, cache.toString(), config).config();
    assertEquals(new Measure(12, "px"), readBack.minContentMargin.top);
    assertEquals(new Measure(1, "cm"), readBack.minContentMargin.left);
    assertNull(readBack.minContentMargin.right);
  }

  /** The C++ suite covers the mode matrix; this only proves the config crosses JNI. */
  @Test
  void viewportModeReachesTheHtml() throws IOException {

    // A text document without margins is reflowing content, so `AUTOMATIC`
    // resolves to `ACTUAL_SIZE`.
    String automatic = renderOdt(new HtmlConfig());
    assertTrue(
        automatic.contains(
            "<meta name=\"viewport\""
                + " content=\"width=device-width,initial-scale=1.0,user-scalable=yes\"/>"));

    HtmlConfig fitWidth = new HtmlConfig();
    fitWidth.viewportMode = HtmlViewportMode.FIT_WIDTH;
    assertTrue(
        renderOdt(fitWidth)
            .contains(
                "<meta name=\"viewport\" content=\"width=device-width,user-scalable=yes\"/>"));

    // only paged content has a width to fit, hence the margins
    HtmlConfig byView = new HtmlConfig();
    byView.viewportMode = HtmlViewportMode.FIT_WIDTH_BY_VIEW;
    byView.textDocumentMargin = true;
    assertTrue(renderOdt(byView).contains("--odr-fit:view"));

    HtmlConfig raw = new HtmlConfig();
    raw.viewportContent = "width=420";
    assertTrue(renderOdt(raw).contains("<meta name=\"viewport\" content=\"width=420\"/>"));
  }

  /** The C++ suite covers what the scheme paints; this only proves it crosses JNI. */
  @Test
  void colorSchemeReachesTheHtml() throws IOException {
    assertEquals(HtmlColorScheme.LIGHT, new HtmlConfig().colorScheme);
    assertTrue(!renderOdt(new HtmlConfig()).contains("prefers-color-scheme"));

    HtmlConfig system = new HtmlConfig();
    system.colorScheme = HtmlColorScheme.SYSTEM;
    assertTrue(renderOdt(system).contains("media=\"(prefers-color-scheme: dark)\""));

    Path cache = Files.createDirectories(tempDir.resolve("scheme"));
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlConfig readBack = Html.translate(file, cache.toString(), system).config();
    assertEquals(HtmlColorScheme.SYSTEM, readBack.colorScheme);
  }

  @Test
  void translateText() throws IOException {
    Html html = translateOffline(TestFiles.txtFile(tempDir));
    List<HtmlPage> pages = html.pages();
    assertEquals(1, pages.size());
    String content = Files.readString(Path.of(pages.get(0).path));
    assertTrue(content.contains("hello text file"));
  }

  @Test
  void translateCsv() throws IOException {
    Html html = translateOffline(TestFiles.csvFile(tempDir));
    // a spreadsheet: a document view plus one per sheet
    assertEquals(2, html.pages().size());
    String content = Files.readString(Path.of(html.pages().get(1).path));
    assertTrue(content.contains("alpha"));
    assertTrue(content.contains("<table"));
  }

  @Test
  void translateDocument() throws IOException {
    Html html = translateOffline(TestFiles.odtFile(tempDir));
    assertEquals(1, html.pages().size());
    String content = Files.readString(Path.of(html.pages().get(0).path));
    assertTrue(content.contains(TestFiles.ODT_WORD));
  }

  /** The C++ suite covers where the limits land; this proves the pair crosses JNI. */
  @Test
  void spreadsheetCutReachesTheView() throws IOException {
    assertEquals(Long.valueOf(500000L), new HtmlConfig().spreadsheetCellLimit);

    Path cache = Files.createDirectories(tempDir.resolve("cache"));
    DecodedFile file = Odr.open(TestFiles.csvFile(tempDir).toString());

    HtmlConfig full = new HtmlConfig();
    for (HtmlView view : Html.translate(file, cache.toString(), full).listViews()) {
      assertNull(view.sheetCut());
    }

    HtmlConfig cut = new HtmlConfig();
    cut.spreadsheetLimit = new TableDimensions(2, 1);
    cut.spreadsheetCellLimit = null;
    HtmlService service = Html.translate(file, cache.toString(), cut);

    assertNull(service.config().spreadsheetCellLimit);
    HtmlSheetCut sheetCut = service.listViews().get(1).sheetCut();
    assertNotNull(sheetCut);
    assertEquals(3, sheetCut.content.rows);
    assertEquals(2, sheetCut.content.columns);
    assertEquals(2, sheetCut.rendered.rows);
    assertEquals(1, sheetCut.rendered.columns);
  }

  @Test
  void htmlServiceViews() throws IOException {
    Path cache = Files.createDirectories(tempDir.resolve("cache"));
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlService service = Html.translate(file, cache.toString(), new HtmlConfig());

    List<HtmlView> views = service.listViews();
    assertEquals(1, views.size());
    assertTrue(service.exists(views.get(0).path()));

    Html.Content content = views.get(0).writeHtml();
    assertTrue(content.html.contains(TestFiles.ODT_WORD));
  }
}
