package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assumptions.assumeTrue;

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

  @Test
  void htmlConfigDefaults() {
    HtmlConfig config = new HtmlConfig();
    assertTrue(config.embedImages);
    assertTrue(!config.editable);
    assertEquals(HtmlTableGridlines.SOFT, config.spreadsheetGridlines);
  }

  @Test
  void translateText() throws IOException {
    assumeTrue(TestFiles.hasCoreData(), "odr core data path not available");
    Html html = translateOffline(TestFiles.txtFile(tempDir));
    List<HtmlPage> pages = html.pages();
    assertEquals(1, pages.size());
    String content = Files.readString(Path.of(pages.get(0).path));
    assertTrue(content.contains("hello text file"));
  }

  @Test
  void translateCsv() throws IOException {
    assumeTrue(TestFiles.hasCoreData(), "odr core data path not available");
    Html html = translateOffline(TestFiles.csvFile(tempDir));
    assertEquals(1, html.pages().size());
    String content = Files.readString(Path.of(html.pages().get(0).path));
    assertTrue(content.contains("alpha"));
  }

  @Test
  void translateDocument() throws IOException {
    assumeTrue(TestFiles.hasCoreData(), "odr core data path not available");
    Html html = translateOffline(TestFiles.odtFile(tempDir));
    assertEquals(1, html.pages().size());
    String content = Files.readString(Path.of(html.pages().get(0).path));
    assertTrue(content.contains(TestFiles.ODT_FIRST_PARAGRAPH));
  }

  @Test
  void htmlServiceViews() throws IOException {
    assumeTrue(TestFiles.hasCoreData(), "odr core data path not available");
    Path cache = Files.createDirectories(tempDir.resolve("cache"));
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlService service = Html.translate(file, cache.toString(), new HtmlConfig());

    List<HtmlView> views = service.listViews();
    assertEquals(1, views.size());
    assertTrue(service.exists(views.get(0).path()));

    Html.Content content = views.get(0).writeHtml();
    assertTrue(content.html.contains(TestFiles.ODT_FIRST_PARAGRAPH));
  }
}
