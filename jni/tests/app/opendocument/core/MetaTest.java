package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.DoubleFunction;
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

    // its own type: an OOXML package we deliberately cannot open
    assertEquals(FileType.EXCEL_BINARY_WORKBOOK, Odr.fileTypeByFileExtension("xlsb"));
    assertFalse(Odr.capabilitiesByFileType(FileType.EXCEL_BINARY_WORKBOOK).open);
  }

  @Test
  void capabilitiesByFileType() {
    FileTypeCapabilities odt = Odr.capabilitiesByFileType(FileType.OPENDOCUMENT_TEXT);
    assertTrue(odt.open);
    assertTrue(odt.translateHtml);
    assertTrue(odt.colorScheme);
    assertTrue(odt.edit);
    assertTrue(odt.create);

    // detected and named, but there is no decoder behind it
    FileTypeCapabilities wpd = Odr.capabilitiesByFileType(FileType.WORD_PERFECT);
    assertTrue(wpd.detectByContent);
    assertFalse(wpd.open);
    assertFalse(wpd.translateHtml);
    assertFalse(wpd.create);

    // a sheet cell can be written, and the package written back
    FileTypeCapabilities ods = Odr.capabilitiesByFileType(FileType.OPENDOCUMENT_SPREADSHEET);
    assertTrue(ods.edit);
    assertTrue(ods.save);

    // a pdf renders, but paints its own page backgrounds
    FileTypeCapabilities pdf = Odr.capabilitiesByFileType(FileType.PORTABLE_DOCUMENT_FORMAT);
    assertTrue(pdf.translateHtml);
    assertTrue(pdf.annotate);
    assertFalse(pdf.colorScheme);
  }

  @Test
  void floatingValuesHaveConsistentEqualityAndHashes() {
    List<DoubleFunction<Object>> values = List.of(
        value -> new Measure(value, "pt"),
        value -> new DrawingPath("M0 0", value, value, value, value),
        value -> new DrawingTransform(value, value, value, value,
            new Measure(0, "pt"), new Measure(0, "pt")));
    for (DoubleFunction<Object> value : values) {
      Object nan = value.apply(Double.NaN);
      Object sameNan = value.apply(Double.NaN);
      assertTrue(nan.equals(nan));
      assertEquals(nan, sameNan);
      assertEquals(nan.hashCode(), sameNan.hashCode());
      Object positiveZero = value.apply(0.0);
      Object negativeZero = value.apply(-0.0);
      assertFalse(positiveZero.equals(negativeZero));
      assertEquals(positiveZero, value.apply(0.0));
      assertEquals(positiveZero.hashCode(), value.apply(0.0).hashCode());
    }
  }

  @Test
  void tablePosition() {
    assertEquals(0, TablePosition.toColumnNum("A"));
    assertEquals("A", TablePosition.toColumnString(0));
    assertEquals("A1", new TablePosition(0, 0).toString());
    assertEquals(Integer.MAX_VALUE, TablePosition.toRowNum("2147483648"));
    assertEquals("2147483648", TablePosition.toRowString(Integer.MAX_VALUE));
    assertThrows(OdrException.class, () -> TablePosition.toRowNum("2147483649"));
    assertThrows(OdrException.class, () -> TablePosition.toColumnString(-1));
    assertThrows(OdrException.class, () -> TablePosition.toRowString(-1));
  }
}
