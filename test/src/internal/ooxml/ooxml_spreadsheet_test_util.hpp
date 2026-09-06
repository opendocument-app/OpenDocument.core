#pragma once

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
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

/// The smallest workbook that opens: one sheet, whose `<sheetData>` is
/// @p sheet_data and which carries @p sheet_extra - `<mergeCells>`, say -
/// after it. @p shared_strings writes a `sharedStrings.xml` where it is given.
inline std::shared_ptr<internal::abstract::File>
workbook(const std::string &sheet_data, const std::string &sheet_extra = "",
         const std::string &shared_strings = "") {
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
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<sheetData>)" +
          sheet_data + R"(</sheetData>)" + sheet_extra + R"(</worksheet>)");

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

inline Document decode(const std::shared_ptr<internal::abstract::File> &file) {
  return Document(
      DecodedFile(internal::open_strategy::open_file(file, {}, Logger::null()))
          .as_document_file()
          .document());
}

inline Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

} // namespace odr::test::ooxml
