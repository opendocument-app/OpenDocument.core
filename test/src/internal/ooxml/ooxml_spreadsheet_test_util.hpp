#pragma once

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <memory>
#include <sstream>
#include <string>

namespace odr::test::ooxml {

inline void insert(internal::zip::ZipArchive &zip, const std::string &path,
                   const std::string &content) {
  zip.insert_file(std::end(zip), internal::RelPath(path),
                  std::make_shared<internal::MemoryFile>(content));
}

/// One sheet with @p sheet_data. @p sheet_prefix goes before `<sheetData>`,
/// @p sheet_extra after it, and @p workbook_extra after `<sheets>`.
inline std::shared_ptr<internal::abstract::File>
workbook(const std::string &sheet_data, const std::string &sheet_extra = "",
         const std::string &shared_strings = "",
         const std::string &workbook_extra = "",
         const std::string &sheet_prefix = "", const std::string &styles = "") {
  internal::zip::ZipArchive zip;
  insert(
      zip, "[Content_Types].xml",
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
      R"(<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
      R"(</Types>)");
  insert(
      zip, "_rels/.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/workbook.xml",
      R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<sheets><sheet name="s" sheetId="1" r:id="rId1"/></sheets>)" +
          workbook_extra + R"(</workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)" +
          styles + R"(</styleSheet>)");
  insert(
      zip, "xl/worksheets/sheet1.xml",
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)" +
          sheet_prefix + R"(<sheetData>)" + sheet_data + R"(</sheetData>)" +
          sheet_extra + R"(</worksheet>)");

  if (!shared_strings.empty()) {
    insert(
        zip, "xl/sharedStrings.xml",
        R"(<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)" +
            shared_strings + R"(</sst>)");
  }

  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

/// One sheet `s` with `drawing1.xml`, `comments1.xml` and `vmlDrawing1.vml`.
inline std::shared_ptr<internal::abstract::File>
workbook_with_parts(const std::string &sheet_data, const std::string &drawing,
                    const std::string &comments, const std::string &notes) {
  internal::zip::ZipArchive zip;
  insert(
      zip, "[Content_Types].xml",
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
      R"(</Types>)");
  insert(
      zip, "_rels/.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/workbook.xml",
      R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<sheets><sheet name="s" sheetId="1" r:id="rId1"/></sheets></workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  insert(
      zip, "xl/worksheets/sheet1.xml",
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<sheetData>)" +
          sheet_data +
          R"(</sheetData><drawing r:id="rId1"/><legacyDrawing r:id="rId3"/>)"
          R"(</worksheet>)");
  insert(
      zip, "xl/worksheets/_rels/sheet1.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing" Target="../drawings/drawing1.xml"/>)"
      R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/comments" Target="../comments1.xml"/>)"
      R"(<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/vmlDrawing" Target="../drawings/vmlDrawing1.vml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/drawings/drawing1.xml",
      R"(<xdr:wsDr xmlns:xdr="http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing">)" +
          drawing + R"(</xdr:wsDr>)");
  insert(
      zip, "xl/comments1.xml",
      R"(<comments xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<authors><author>a</author></authors><commentList>)" +
          comments + R"(</commentList></comments>)");
  insert(zip, "xl/drawings/vmlDrawing1.vml",
         R"(<xml xmlns:v="urn:schemas-microsoft-com:vml" )"
         R"(xmlns:x="urn:schemas-microsoft-com:office:excel">)" +
             notes + R"(</xml>)");

  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

inline Document decode(const std::shared_ptr<internal::abstract::File> &file) {
  return Document(
      DecodedFile(internal::open_strategy::open_file(file, {}, Logger::null()))
          .as_document_file()
          .document());
}

inline Document saved_document(const Document &document) {
  std::ostringstream saved;
  document.save(saved);
  return decode(std::make_shared<internal::MemoryFile>(saved.str()));
}

inline std::string saved_part(const Document &document,
                              const std::string &path) {
  std::ostringstream xml;
  xml << saved_document(document).as_filesystem().open(path).stream()->rdbuf();
  return xml.str();
}

inline Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

} // namespace odr::test::ooxml
