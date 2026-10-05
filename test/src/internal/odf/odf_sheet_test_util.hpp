#pragma once

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <cstdint>
#include <sstream>
#include <string>

namespace odr::test::odf {

/// Flat spreadsheet tables followed by optional named expressions or settings.
inline std::string flat_spreadsheet(const std::string &tables,
                                    const std::string &body = "") {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:body><office:spreadsheet>)" +
         tables + body +
         R"(</office:spreadsheet></office:body></office:document>)";
}

inline std::string table(const std::string &name, const std::string &rows) {
  return R"(<table:table table:name=")" + name + R"(">)" + rows +
         R"(</table:table>)";
}

inline std::string row(const std::string &cells,
                       const std::uint32_t repeated = 1) {
  return R"(<table:table-row)" +
         (repeated > 1 ? R"( table:number-rows-repeated=")" +
                             std::to_string(repeated) + R"(")"
                       : std::string()) +
         ">" + cells + R"(</table:table-row>)";
}

inline std::string string_cell(const std::string &text) {
  return R"(<table:table-cell office:value-type="string"><text:p>)" + text +
         R"(</text:p></table:table-cell>)";
}

inline std::string formula_cell(const std::string &formula) {
  return R"(<table:table-cell table:formula=")" + formula +
         R"(" office:value-type="float" office:value="1"><text:p>1</text:p>)"
         R"(</table:table-cell>)";
}

inline Document document_of(const std::string &source) {
  return open(File::from_memory(source)).as_document_file().document();
}

inline Sheet sheet_at(const Document &document, const std::uint32_t index) {
  auto it = document.root_element().children().begin();
  for (std::uint32_t i = 0; i < index; ++i) {
    ++it;
  }
  return (*it).as_sheet();
}

inline std::string text_at(const Sheet &sheet, const std::uint32_t column,
                           const std::uint32_t row) {
  return sheet.cell(column, row).value().text();
}

inline std::string saved(const Document &document) {
  std::ostringstream out;
  document.save(out);
  return out.str();
}

} // namespace odr::test::odf
