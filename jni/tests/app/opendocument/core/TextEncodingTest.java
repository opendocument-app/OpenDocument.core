package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Arrays;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class TextEncodingTest {
  @TempDir Path tempDir;

  @Test
  void ordinalsMatchTheNativeEnum() {
    // the mapping is by ordinal, so the two ends have to agree on the first
    assertEquals(0, TextEncoding.UNKNOWN.ordinal());
    assertEquals("UTF-8", TextEncoding.UTF8.canonicalName());
  }

  @Test
  void namesResolveBothWays() {
    assertEquals("windows-1252", TextEncoding.WINDOWS_1252.canonicalName());
    // case and separators are ignored, so an alias resolves
    assertEquals(TextEncoding.WINDOWS_1252, TextEncoding.byName("CP1252"));
    assertEquals(TextEncoding.UNKNOWN, TextEncoding.byName("nope"));
    assertTrue(Arrays.asList(TextEncoding.WINDOWS_1252.names()).contains("windows-1252"));
  }

  @Test
  void unknownHasNoName() {
    assertThrows(OdrException.class, TextEncoding.UNKNOWN::canonicalName);
  }

  @Test
  void decodableIsNarrowerThanNamed() {
    assertTrue(TextEncoding.UTF8.isDecodable());
    // named so a caller can say what a file is, but not decoded here
    assertFalse(TextEncoding.SHIFT_JIS.isDecodable());
  }

  @Test
  void allLeavesOutUnknown() {
    TextEncoding[] all = TextEncoding.all();
    assertTrue(Arrays.asList(all).contains(TextEncoding.UTF8));
    assertFalse(Arrays.asList(all).contains(TextEncoding.UNKNOWN));
  }

  @Test
  void aTextFileReportsItsEncoding() throws IOException {
    Path path = Files.writeString(tempDir.resolve("plain.txt"), "hello");

    try (DecodedFile decoded = Odr.open(path.toString())) {
      TextFile file = decoded.asTextFile();
      assertEquals(TextEncoding.UTF8, file.encoding());
      assertEquals("UTF-8", file.charset());
    }
  }
}
