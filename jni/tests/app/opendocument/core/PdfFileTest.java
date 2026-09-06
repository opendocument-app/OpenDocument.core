package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class PdfFileTest {
  @TempDir Path tempDir;

  private static final String HIGHLIGHT =
      "{\"version\": 1, \"annotations\": [{\"page\": 0, \"type\": \"highlight\","
          + " \"quads\": [[72, 700, 300, 700, 72, 688, 300, 688]],"
          + " \"color\": [1, 0.9, 0.2]}]}";

  @Test
  void annotateIsDeclaredForPdf() {
    assertTrue(Odr.capabilitiesByFileType(FileType.PORTABLE_DOCUMENT_FORMAT).annotate);
  }

  @Test
  void annotateAppendsToTheSource() throws IOException {
    Path pdf = TestFiles.pdfFile(tempDir);
    byte[] source = Files.readAllBytes(pdf);

    try (DecodedFile file = Odr.open(pdf.toString())) {
      byte[] result = file.asPdfFile().annotate(HIGHLIGHT);

      assertTrue(result.length > source.length);
      // the source is copied and the annotation appended after it
      for (int i = 0; i < source.length; ++i) {
        assertEquals(source[i], result[i]);
      }
      String text = new String(result, StandardCharsets.ISO_8859_1);
      assertTrue(text.contains("/Highlight"));
      assertTrue(text.contains("/Subtype /Form"));
    }
  }

  @Test
  void annotateRefusesAPayloadItDoesNotUnderstand() throws IOException {
    Path pdf = TestFiles.pdfFile(tempDir);
    try (DecodedFile file = Odr.open(pdf.toString())) {
      PdfFile pdfFile = file.asPdfFile();
      assertThrows(OdrException.class, () -> pdfFile.annotate("{\"version\": 2}"));
      assertThrows(OdrException.class, () -> pdfFile.annotate("not json"));
    }
  }
}
