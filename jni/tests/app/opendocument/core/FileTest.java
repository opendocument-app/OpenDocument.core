package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class FileTest {
  @TempDir Path tempDir;

  @Test
  void openOdt() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    try (DecodedFile file = Odr.open(odt.toString())) {
      assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileType());
      assertEquals(FileCategory.DOCUMENT, file.fileCategory());
      assertTrue(file.isDocumentFile());
      assertFalse(file.passwordEncrypted());

      DocumentFile documentFile = file.asDocumentFile();
      assertEquals(DocumentType.TEXT, documentFile.documentType());
      assertEquals(DocumentType.TEXT, documentFile.fileMeta().documentType);
    }
  }

  @Test
  void thumbnail() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    try (DecodedFile file = Odr.open(odt.toString())) {
      try (File thumbnail = file.asDocumentFile().thumbnail()) {
        assertNotNull(thumbnail);
        assertTrue(thumbnail.read().length > 0);
      }
    }
  }

  @Test
  void openText() throws IOException {
    Path txt = TestFiles.txtFile(tempDir);
    try (DecodedFile file = Odr.open(txt.toString())) {
      assertEquals(FileType.TEXT_FILE, file.fileType());
      assertTrue(file.isTextFile());
      // The payload carries a non-BMP character, so this also covers the
      // UTF-8 -> UTF-16 conversion of the JNI string helpers.
      assertEquals(TestFiles.TXT_CONTENT, file.asTextFile().text());
    }
  }

  @Test
  void openCsv() throws IOException {
    Path csv = TestFiles.csvFile(tempDir);
    try (DecodedFile file = Odr.open(csv.toString())) {
      assertEquals(FileType.COMMA_SEPARATED_VALUES, file.fileType());
      // A csv holds a text file rather than being one; both views stay open.
      assertFalse(file.isTextFile());
      assertTrue(file.isCsvFile());

      CsvFile decodedCsv = file.asCsvFile();
      assertTrue(decodedCsv.textFile().text().startsWith("name,"));
      assertNotNull(decodedCsv.document().rootElement());
    }
  }

  @Test
  void textFileWritesAnEditBack() throws IOException {
    Path txt = TestFiles.txtFile(tempDir);
    try (DecodedFile file = Odr.open(txt.toString())) {
      TextFile text = file.asTextFile();
      assertTrue(text.isSavable());

      byte[] edited =
          text.writeEdited(
              "{\"version\":2,\"ops\":[{\"op\":\"setContent\",\"text\":\"rewritten\"}]}");

      assertEquals("rewritten", new String(edited, java.nio.charset.StandardCharsets.UTF_8));
    }
  }

  @Test
  void fileReadMatchesSize() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    try (File file = new File(odt.toString())) {
      assertEquals(FileLocation.DISK, file.location());
      assertNotNull(file.diskPath());
      assertTrue(file.size() > 0);
      assertEquals(file.size(), file.read().length);
    }
  }

  @Test
  void fileName() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    try (File file = new File(odt.toString())) {
      assertEquals(TestFiles.ODT_RESOURCE, file.name());
    }
  }

  /** A file inside a package is named by its entry, not by the package. */
  @Test
  void archiveEntryName() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    try (DecodedFile file = Odr.open(odt.toString(), FileType.ZIP)) {
      Filesystem filesystem = file.asArchiveFile().archive().asFilesystem();
      try (File entry = filesystem.open("/content.xml")) {
        assertEquals("content.xml", entry.name());
      }
    }
  }

  @Test
  void decodeAnOpenFile() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    try (File file = new File(odt.toString());
        DecodedFile decoded = Odr.open(file)) {
      assertEquals(FileType.OPENDOCUMENT_TEXT, decoded.fileType());
    }
  }

  @Test
  void fileMeta() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    try (DecodedFile file = Odr.open(odt.toString())) {
      FileMeta meta = file.fileMeta();
      assertEquals(FileType.OPENDOCUMENT_TEXT, meta.type);
      assertFalse(meta.passwordEncrypted);
      assertEquals(DocumentType.TEXT, meta.documentType);
    }
  }

  @Test
  void openMissingFileThrows() {
    assertThrows(
        OdrException.FileNotFound.class,
        () -> Odr.open(tempDir.resolve("missing.odt").toString()));
  }

  @Test
  void openCarriesCsvOptions() throws IOException {
    Path path = Files.writeString(tempDir.resolve("semicolons.csv"), "a;b\n1;2\n");

    // detection would find the semicolon; a pipe it would not, so the caller says
    DecodeOptions options = new DecodeOptions();
    options.asFileType = FileType.COMMA_SEPARATED_VALUES;
    options.csv.separator = '|';

    try (DecodedFile file = Odr.open(path.toString(), options)) {
      assertEquals(FileType.COMMA_SEPARATED_VALUES, file.fileType());
    }
  }

  @Test
  void listFileTypes() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    assertTrue(Odr.listFileTypes(odt.toString()).contains(FileType.OPENDOCUMENT_TEXT));
  }

  @Test
  void documentFileByPath() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    DecodedFile file = Odr.open(odt.toString());
    assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileType());
    assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileMeta().type);
  }
}
