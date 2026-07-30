package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
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
  void aliasesRoundTrip() {
    List<FileType> fileTypes = Odr.allFileTypes();
    assertTrue(fileTypes.contains(FileType.OPENDOCUMENT_TEXT));

    Set<String> seen = new HashSet<>();
    for (FileType fileType : fileTypes) {
      for (String extension : Odr.fileExtensionsByFileType(fileType)) {
        assertTrue(seen.add("ext:" + extension), extension);
        assertEquals(fileType, Odr.fileTypeByFileExtension(extension));
      }
      for (String mimetype : Odr.mimetypesByFileType(fileType)) {
        assertTrue(seen.add("mime:" + mimetype), mimetype);
        assertEquals(fileType, Odr.fileTypeByMimetype(mimetype));
      }
    }

    assertEquals(
        FileType.OFFICE_OPEN_XML_DOCUMENT, Odr.fileTypeByFileExtension("docm"));
    assertEquals("odt", Odr.fileExtensionByFileType(FileType.OPENDOCUMENT_TEXT));
  }

  @Test
  void capabilitiesByFileType() {
    FileTypeCapabilities odt = Odr.capabilitiesByFileType(FileType.OPENDOCUMENT_TEXT);
    assertTrue(odt.open);
    assertTrue(odt.translateHtml);
    assertTrue(odt.edit);

    // detected and named, but there is no decoder behind it
    FileTypeCapabilities wpd = Odr.capabilitiesByFileType(FileType.WORD_PERFECT);
    assertTrue(wpd.detectByContent);
    assertFalse(wpd.open);
    assertFalse(wpd.translateHtml);

    // spreadsheet editing is force-disabled
    assertFalse(Odr.capabilitiesByFileType(FileType.OPENDOCUMENT_SPREADSHEET).edit);
  }

  @Test
  void tablePosition() {
    assertEquals(0, TablePosition.toColumnNum("A"));
    assertEquals("A", TablePosition.toColumnString(0));
    assertEquals("A1", new TablePosition(0, 0).toString());
  }
}
