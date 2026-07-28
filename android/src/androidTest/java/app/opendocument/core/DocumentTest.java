package app.opendocument.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Decoding and rendering on a device, mirroring the host suite in {@code jni/tests}. What is android
 * specific here is the runtime: every one of these calls crosses into the same java API on an
 * android class library, where a method the JDK has and android does not fails only when it is
 * reached (#621).
 */
@RunWith(AndroidJUnit4.class)
public class DocumentTest {
  private Path tempDir;

  @Before
  public void setUp() throws IOException {
    TestSupport.initialize();
    tempDir = TestSupport.tempDir("document");
  }

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
  public void elementTree() throws IOException {
    Document document = openDocument();
    assertEquals(DocumentType.TEXT, document.documentType());
    assertEquals(FileType.OPENDOCUMENT_TEXT, document.fileType());

    Element root = document.rootElement();
    assertEquals(ElementType.ROOT, root.type());

    List<String> text = walkText(root);
    assertTrue(text.contains(TestFiles.ODT_FIRST_PARAGRAPH));
    // exercises non-BMP characters across the JNI string conversion
    assertTrue(text.contains(TestFiles.ODT_SECOND_PARAGRAPH));
  }

  @Test
  public void elementNavigation() throws IOException {
    Element root = openDocument().rootElement();

    Element first = root.firstChild();
    assertNotNull(first);
    assertTrue(first.parent().isSame(root));
    Element second = first.nextSibling();
    assertNotNull(second);
    assertTrue(second.previousSibling().isSame(first));
    assertNotNull(first.documentPath().toString());
  }

  @Test
  public void documentFilesystem() throws IOException {
    assertTrue(openDocument().asFilesystem().isFile("/content.xml"));
  }

  @Test
  public void fileMeta() throws IOException {
    try (DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString())) {
      FileMeta meta = file.fileMeta();
      assertEquals(FileType.OPENDOCUMENT_TEXT, meta.type);
      assertFalse(meta.passwordEncrypted);
      assertEquals(EncryptionState.NOT_ENCRYPTED, file.encryptionState());
    }
  }

  @Test
  public void translateToHtml() throws IOException {
    Path cache = Files.createDirectories(tempDir.resolve("cache"));
    Path output = Files.createDirectories(tempDir.resolve("output"));

    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlService service = Html.translate(file, cache.toString(), new HtmlConfig());
    Html html = service.bringOffline(output.toString());

    List<HtmlPage> pages = html.pages();
    assertEquals(1, pages.size());
    // the renderer reads the css/js the AAR ships, so this only passes with the
    // extracted assets in place
    String content = read(Paths.get(pages.get(0).path));
    assertTrue(content.contains(TestFiles.ODT_FIRST_PARAGRAPH));
  }

  @Test
  public void translateCsv() throws IOException {
    Path cache = Files.createDirectories(tempDir.resolve("csv-cache"));
    DecodedFile file = Odr.open(TestFiles.csvFile(tempDir).toString());
    HtmlService service = Html.translate(file, cache.toString(), new HtmlConfig());

    List<HtmlView> views = service.listViews();
    assertEquals(1, views.size());
    assertTrue(views.get(0).writeHtml().html.contains("alpha"));
  }

  private static String read(Path path) throws IOException {
    return new String(Files.readAllBytes(path), StandardCharsets.UTF_8);
  }
}
