package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.Test;

class MetaTest {
  @Test
  void version() {
    // The version is injected by the packaging build and empty in dev builds.
    assertNotNull(Odr.version());
    assertNotNull(Odr.commitHash());
    assertFalse(Odr.identify().isEmpty());
  }

  @Test
  void fileTypeByFileExtension() {
    assertEquals(FileType.OPENDOCUMENT_TEXT, Odr.fileTypeByFileExtension("odt"));
    assertEquals(FileType.PORTABLE_DOCUMENT_FORMAT, Odr.fileTypeByFileExtension("pdf"));
  }

  @Test
  void fileTypeStrings() {
    String string = Odr.fileTypeToString(FileType.OPENDOCUMENT_TEXT);
    assertFalse(string.isEmpty());
    assertEquals(
        FileCategory.DOCUMENT, Odr.fileCategoryByFileType(FileType.OPENDOCUMENT_TEXT));
    assertEquals(DocumentType.TEXT, Odr.documentTypeByFileType(FileType.OPENDOCUMENT_TEXT));
  }

  @Test
  void mimetypeRoundTrip() {
    String mimetype = Odr.mimetypeByFileType(FileType.OPENDOCUMENT_TEXT);
    assertFalse(mimetype.isEmpty());
    assertEquals(FileType.OPENDOCUMENT_TEXT, Odr.fileTypeByMimetype(mimetype));
  }

  @Test
  void tablePosition() {
    assertEquals(0, TablePosition.toColumnNum("A"));
    assertEquals("A", TablePosition.toColumnString(0));
    assertEquals("A1", new TablePosition(0, 0).toString());
  }
}
