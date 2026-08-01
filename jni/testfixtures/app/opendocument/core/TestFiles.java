package app.opendocument.core;

import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.Arrays;
import java.util.List;

/**
 * The suite's input files.
 *
 * <p>The document is {@code odt/mixed-layout.odt} from <a
 * href="https://github.com/opendocument-app/OpenDocument.test">OpenDocument.test</a>, carried here
 * as a resource next to this class: {@code test/data/} is fetched by {@code cmake/test_data.cmake}
 * and an android build tree never sees it. 9 KB of real LibreOffice output, and it replaced a
 * document this fixture built with {@code ZipOutputStream} — which only ever proved that odrcore
 * could read back what the test had written. Text and CSV need no container, so those stay inline.
 *
 * <p>Shared by the host junit suite ({@code jni/tests}) and the instrumented suite of the AAR
 * ({@code android/}), so it stays within what android API 26 offers — no {@code Path.of}, no {@code
 * Files.writeString}, no {@code String.formatted}.
 */
final class TestFiles {
  static final String ODT_RESOURCE = "mixed-layout.odt";

  /**
   * The text of the document, node by node in document order. Each paragraph is a run and a span,
   * so the numbers are their own text elements — and the runs keep their trailing space.
   */
  static final List<String> ODT_TEXT =
      Arrays.asList("Portrait ", "1", "Portrait ", "2", "Landscape ", "1", "Portrait ", "3");

  /**
   * A word of the document, found in the element tree and in the rendered HTML alike. A whole run
   * would not do for the latter: the renderer writes its trailing space as {@code &nbsp;}.
   */
  static final String ODT_WORD = "Landscape";

  /** Non-BMP, to exercise the UTF-8 ↔ UTF-16 conversion of the JNI string helpers. */
  static final String TXT_CONTENT = "hello text file\nsecond line äöü 😀\n";

  /** The OpenDocument text file, unpacked from the classpath into {@code directory}. */
  static Path odtFile(Path directory) throws IOException {
    Path path = directory.resolve(ODT_RESOURCE);
    try (InputStream stream = TestFiles.class.getResourceAsStream(ODT_RESOURCE)) {
      if (stream == null) {
        throw new IOException(ODT_RESOURCE + " is missing from the test classpath");
      }
      Files.copy(stream, path, StandardCopyOption.REPLACE_EXISTING);
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
    write(path, TXT_CONTENT);
    return path;
  }

  private static void write(Path path, String content) throws IOException {
    Files.write(path, content.getBytes(StandardCharsets.UTF_8));
  }

  private TestFiles() {}
}
