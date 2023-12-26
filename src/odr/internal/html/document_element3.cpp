#include <odr/internal/html/document_element.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/html/common.hpp>
#include <odr/internal/html/document_style.hpp>
#include <odr/internal/html/html_writer.hpp>

namespace odr::internal {

void html::translate_sheet(Element element, HtmlWriter &out,
                           const HtmlConfig &config) {
  Sheet sheet = element.sheet();

  out.write_element_begin(
      "table",
      {.attributes = HtmlAttributesVector{
           {"cellpadding", "0"}, {"border", "0"}, {"cellspacing", "0"}}});

  TableDimensions dimensions = sheet.dimensions();
  std::uint32_t end_column = dimensions.columns;
  std::uint32_t end_row = dimensions.rows;
  if (config.spreadsheet_limit_by_content) {
    TableDimensions content = sheet.content(config.spreadsheet_limit);
    end_column = content.columns;
    end_row = content.rows;
  }
  if (config.spreadsheet_limit) {
    end_column = std::min(end_column, config.spreadsheet_limit->columns);
    end_row = std::min(end_row, config.spreadsheet_limit->rows);
  }
  end_column = std::max(1u, end_column);
  end_row = std::max(1u, end_row);

  out.write_element_begin("col", {.close_type = HtmlCloseType::none});

  for (std::uint32_t column_index = 0; column_index < end_column;
       ++column_index) {
    SheetColumn table_column = sheet.column(column_index);
    TableColumnStyle table_column_style = table_column.style();

    out.write_element_begin(
        "col", {.close_type = HtmlCloseType::none,
                .style = translate_table_column_style(table_column_style)});
  }

  {
    out.write_element_begin("tr");

    out.write_element_begin("td", {.close_type = HtmlCloseType::trailing,
                                   .style = "width:30px;height:20px;"});

    for (std::uint32_t column_index = 0; column_index < end_column;
         ++column_index) {
      out.write_element_begin(
          "td", {.inline_element = true,
                 .style = "text-align:center;vertical-align:middle;"});
      out.write_raw(common::TablePosition::to_column_string(column_index));
      out.write_element_end("td");
    }

    out.write_element_end("tr");
  }

  common::TableCursor cursor;
  for (std::uint32_t row_index = cursor.row(); row_index < end_row;
       row_index = cursor.row()) {
    SheetRow table_row = sheet.row(row_index);
    TableRowStyle table_row_style = table_row.style();

    out.write_element_begin(
        "tr", {.style = translate_table_row_style(table_row_style)});

    out.write_element_begin(
        "td", {.inline_element = true, .style = [&]() {
                 std::string style = "text-align:center;vertical-align:middle;";
                 if (std::optional<Measure> height = table_row_style.height) {
                   style += "height:" + height->to_string() + ";";
                   style += "max-height:" + height->to_string() + ";";
                 }
                 return style;
               }()});
    out.write_raw(common::TablePosition::to_row_string(row_index));
    out.write_element_end("td");

    for (std::uint32_t column_index = cursor.column();
         column_index < end_column; column_index = cursor.column()) {
      SheetCell cell = sheet.cell(column_index, row_index);

      if (cell.is_covered()) {
        continue;
      }

      // TODO looks a bit odd to query the same (col, row) all the time. maybe
      // there could be a struct to get all the info?
      TableCellStyle cell_style = cell.style();
      TableDimensions cell_span = cell.span();
      ValueType cell_value_type = cell.value_type();

      auto attributes = [&](const HtmlAttributeWriterCallback &clb) {
        if (cell_span.columns > 1) {
          clb("colspan", std::to_string(cell_span.columns));
        }
        if (cell_span.rows > 1) {
          clb("rowspan", std::to_string(cell_span.rows));
        }
      };
      auto clazz = [&]() -> std::optional<HtmlWritable> {
        if (cell_value_type == ValueType::float_number) {
          return "odr-value-type-float";
        }
        return std::nullopt;
      }();
      out.write_element_begin("td",
                              {.attributes = attributes,
                               .style = translate_table_cell_style(cell_style),
                               .clazz = clazz});
      if ((column_index == 0) && (row_index == 0)) {
        for (Element shape : sheet.shapes()) {
          translate_element(shape, out, config);
        }
      }
      translate_children(cell.children(), out, config);
      out.write_element_end("td");

      cursor.add_cell(cell_span.columns, cell_span.rows);
    }

    out.write_element_end("tr");

    cursor.add_row();
  }

  out.write_element_end("table");

  return;
}

} // namespace odr::internal
