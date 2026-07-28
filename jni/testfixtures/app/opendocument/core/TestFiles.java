package app.opendocument.core;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/**
 * Builds minimal test input files from inline content (mirrors python/tests/conftest.py).
 *
 * <p>Shared by the host junit suite ({@code jni/tests}) and the instrumented suite of the AAR
 * ({@code android/}), so it stays within what android API 26 offers — no {@code Path.of}, no {@code
 * Files.writeString}, no {@code String.formatted}.
 */
final class TestFiles {
  static final String ODT_FIRST_PARAGRAPH = "Hello from odr-core-java!";
  static final String ODT_SECOND_PARAGRAPH = "Second paragraph äöü 😀";

  private static final String ODT_CONTENT_XML =
      """
      <?xml version="1.0" encoding="UTF-8"?>
      <office:document-content
          xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
          xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
          office:version="1.2">
        <office:body>
          <office:text>
            <text:p>%s</text:p>
            <text:p>%s</text:p>
          </office:text>
        </office:body>
      </office:document-content>
      """;

  private static final String ODT_CONTENT =
      String.format(ODT_CONTENT_XML, ODT_FIRST_PARAGRAPH, ODT_SECOND_PARAGRAPH);

  private static final String ODT_STYLES_XML =
      """
      <?xml version="1.0" encoding="UTF-8"?>
      <office:document-styles
          xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
          office:version="1.2">
        <office:styles/>
        <office:automatic-styles/>
        <office:master-styles/>
      </office:document-styles>
      """;

  private static final String ODT_MANIFEST_XML =
      """
      <?xml version="1.0" encoding="UTF-8"?>
      <manifest:manifest
          xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0"
          manifest:version="1.2">
        <manifest:file-entry manifest:full-path="/"
            manifest:media-type="application/vnd.oasis.opendocument.text"/>
        <manifest:file-entry manifest:full-path="content.xml"
            manifest:media-type="text/xml"/>
        <manifest:file-entry manifest:full-path="styles.xml"
            manifest:media-type="text/xml"/>
      </manifest:manifest>
      """;

  static {
    // Mirror the python conftest: pick up the shipped assets for rendering.
    String dataPath = System.getenv("ODR_CORE_DATA_PATH");
    if (dataPath != null && !dataPath.isEmpty()) {
      GlobalParams.setOdrCoreDataPath(dataPath);
    }
  }

  /** Whether the odr core data (CSS/JS assets) is available for rendering. */
  static boolean hasCoreData() {
    String path = GlobalParams.odrCoreDataPath();
    return path != null && !path.isEmpty() && Files.isDirectory(Paths.get(path));
  }

  /** A minimal OpenDocument text file built from inline XML. */
  static Path odtFile(Path directory) throws IOException {
    Path path = directory.resolve("minimal.odt");
    try (ZipOutputStream zip = new ZipOutputStream(Files.newOutputStream(path))) {
      byte[] mimetype =
          "application/vnd.oasis.opendocument.text".getBytes(StandardCharsets.UTF_8);
      ZipEntry entry = new ZipEntry("mimetype");
      entry.setMethod(ZipEntry.STORED);
      entry.setSize(mimetype.length);
      CRC32 crc = new CRC32();
      crc.update(mimetype);
      entry.setCrc(crc.getValue());
      zip.putNextEntry(entry);
      zip.write(mimetype);
      zip.closeEntry();

      writeEntry(zip, "content.xml", ODT_CONTENT);
      writeEntry(zip, "styles.xml", ODT_STYLES_XML);
      writeEntry(zip, "META-INF/manifest.xml", ODT_MANIFEST_XML);
    }
    return path;
  }

  static Path csvFile(Path directory) throws IOException {
    Path path = directory.resolve("table.csv");
    write(path, "name,value\nalpha,1\nbeta,2\n");
    return path;
  }

  static Path txtFile(Path directory) throws IOException {
    Path path = directory.resolve("note.txt");
    write(path, "hello text file\nsecond line\n");
    return path;
  }

  private static void write(Path path, String content) throws IOException {
    Files.write(path, content.getBytes(StandardCharsets.UTF_8));
  }

  private static void writeEntry(ZipOutputStream zip, String name, String content)
      throws IOException {
    zip.putNextEntry(new ZipEntry(name));
    zip.write(content.getBytes(StandardCharsets.UTF_8));
    zip.closeEntry();
  }

  private TestFiles() {}
}
