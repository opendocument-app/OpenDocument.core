package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
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
  }

  @Test
  void viewportConfigRoundTrips() throws IOException {
    HtmlConfig config = new HtmlConfig();
    config.viewportMode = HtmlViewportMode.FIT_WIDTH;
    config.spreadsheetViewportMode = HtmlViewportMode.ACTUAL_SIZE;
    config.viewportContent = "width=420";

    Path cache = Files.createDirectories(tempDir.resolve("cache"));
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlConfig readBack = Html.translate(file, cache.toString(), config).config();

    assertEquals(HtmlViewportMode.FIT_WIDTH, readBack.viewportMode);
    assertEquals(HtmlViewportMode.ACTUAL_SIZE, readBack.spreadsheetViewportMode);
    assertEquals("width=420", readBack.viewportContent);
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

    HtmlConfig raw = new HtmlConfig();
    raw.viewportContent = "width=420";
    assertTrue(renderOdt(raw).contains("<meta name=\"viewport\" content=\"width=420\"/>"));
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
