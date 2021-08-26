#include <fstream>
#include <internal/abstract/document.h>
#include <internal/common/file.h>
#include <internal/common/html.h>
#include <internal/common/table_cursor.h>
#include <internal/crypto/crypto_util.h>
#include <internal/svm/svm_file.h>
#include <internal/svm/svm_to_svg.h>
#include <internal/util/stream_util.h>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>
#include <sstream>

using namespace odr::internal;

namespace odr {

namespace {
void translate_children(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig &config);
void translate_element(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config);

std::string translate_outer_page_style(const ElementPropertySet &properties) {
  std::string result;
  if (auto width = properties.get_string(ElementProperty::WIDTH)) {
    result += "width:" + *width + ";";
  }
  if (auto height = properties.get_string(ElementProperty::HEIGHT)) {
    result += "height:" + *height + ";";
  }
  return result;
}

std::string translate_inner_page_style(const ElementPropertySet &properties) {
  std::string result;
  if (auto margin_top = properties.get_string(ElementProperty::MARGIN_TOP)) {
    result += "margin-top:" + *margin_top + ";";
  }
  if (auto margin_left = properties.get_string(ElementProperty::MARGIN_LEFT)) {
    result += "margin-left:" + *margin_left + ";";
  }
  if (auto margin_bottom =
          properties.get_string(ElementProperty::MARGIN_BOTTOM)) {
    result += "margin-bottom:" + *margin_bottom + ";";
  }
  if (auto margin_right =
          properties.get_string(ElementProperty::MARGIN_RIGHT)) {
    result += "margin-right:" + *margin_right + ";";
  }
  return result;
}

std::string translate_paragraph_style(const ElementPropertySet &properties) {
  std::string result;
  if (auto text_align = properties.get_string(ElementProperty::TEXT_ALIGN)) {
    result += "text-align:" + *text_align + ";";
  }
  if (auto margin_top = properties.get_string(ElementProperty::MARGIN_TOP)) {
    result += "margin-top:" + *margin_top + ";";
  }
  if (auto margin_bottom =
          properties.get_string(ElementProperty::MARGIN_BOTTOM)) {
    result += "margin-bottom:" + *margin_bottom + ";";
  }
  if (auto margin_left = properties.get_string(ElementProperty::MARGIN_LEFT)) {
    result += "margin-left:" + *margin_left + ";";
  }
  if (auto margin_right =
          properties.get_string(ElementProperty::MARGIN_RIGHT)) {
    result += "margin-right:" + *margin_right + ";";
  }
  return result;
}

std::string translate_text_style(const ElementPropertySet &properties) {
  std::string result;
  if (auto font_name = properties.get_string(ElementProperty::FONT_NAME)) {
    result += "font-family:" + *font_name + ";";
  }
  if (auto font_size = properties.get_string(ElementProperty::FONT_SIZE);
      font_size) {
    result += "font-size:" + *font_size + ";";
  }
  if (auto font_weight = properties.get_string(ElementProperty::FONT_WEIGHT)) {
    result += "font-weight:" + *font_weight + ";";
  }
  if (auto font_style = properties.get_string(ElementProperty::FONT_STYLE)) {
    result += "font-style:" + *font_style + ";";
  }
  if (auto underline = properties.get_string(ElementProperty::FONT_UNDERLINE);
      underline && underline == "solid") {
    result += "text-decoration:underline;";
  }
  if (auto strikethrough =
          properties.get_string(ElementProperty::FONT_STRIKETHROUGH);
      strikethrough && strikethrough == "solid") {
    result += "text-decoration:line-through;";
  }
  if (auto font_shadow = properties.get_string(ElementProperty::FONT_SHADOW)) {
    result += "text-shadow:" + *font_shadow + ";";
  }
  if (auto font_color = properties.get_string(ElementProperty::FONT_COLOR)) {
    result += "color:" + *font_color + ";";
  }
  if (auto background_color =
          properties.get_string(ElementProperty::BACKGROUND_COLOR);
      background_color && background_color != "transparent") {
    result += "background-color:" + *background_color + ";";
  }
  return result;
}

std::string translate_table_style(const ElementPropertySet &properties) {
  std::string result;
  if (auto width = properties.get_string(ElementProperty::WIDTH); width) {
    result += "width:" + *width + ";";
  }
  return result;
}

std::string translate_table_column_style(const ElementPropertySet &properties) {
  std::string result;
  if (auto width = properties.get_string(ElementProperty::WIDTH)) {
    result += "width:" + *width + ";";
  }
  return result;
}

std::string translate_table_row_style(const ElementPropertySet &properties) {
  std::string result;
  // TODO that does not work with HTML; height would need to be applied to the
  // cells
  if (auto height = properties.get_string(ElementProperty::HEIGHT)) {
    result += "height:" + *height + ";";
  }
  return result;
}

std::string translate_table_cell_style(const ElementPropertySet &properties) {
  std::string result;

  result += translate_text_style(properties);
  result += translate_paragraph_style(properties);

  // TODO check value type for alignment

  if (auto vertical_align =
          properties.get_string(ElementProperty::VERTICAL_ALIGN)) {
    // TODO
  }
  if (auto background_color =
          properties.get_string(ElementProperty::TABLE_CELL_BACKGROUND_COLOR);
      background_color && *background_color != "transparent") {
    result += "background-color:" + *background_color + ";";
  }
  if (auto padding_top = properties.get_string(ElementProperty::PADDING_TOP)) {
    result += "padding-top:" + *padding_top + ";";
  }
  if (auto padding_bottom =
          properties.get_string(ElementProperty::PADDING_BOTTOM)) {
    result += "padding-bottom:" + *padding_bottom + ";";
  }
  if (auto padding_left =
          properties.get_string(ElementProperty::PADDING_LEFT)) {
    result += "padding-left:" + *padding_left + ";";
  }
  if (auto padding_right =
          properties.get_string(ElementProperty::PADDING_RIGHT)) {
    result += "padding-right:" + *padding_right + ";";
  }
  if (auto border_top = properties.get_string(ElementProperty::BORDER_TOP)) {
    result += "border-top:" + *border_top + ";";
  }
  if (auto border_bottom =
          properties.get_string(ElementProperty::BORDER_BOTTOM)) {
    result += "border-bottom:" + *border_bottom + ";";
  }
  if (auto border_left = properties.get_string(ElementProperty::BORDER_LEFT)) {
    result += "border-left:" + *border_left + ";";
  }
  if (auto border_right =
          properties.get_string(ElementProperty::BORDER_RIGHT)) {
    result += "border-right:" + *border_right + ";";
  }
  return result;
}

std::string translate_drawing_style(const ElementPropertySet &properties) {
  std::string result;
  if (auto stroke_width =
          properties.get_string(ElementProperty::STROKE_WIDTH)) {
    result += "stroke-width:" + *stroke_width + ";";
  }
  if (auto stroke_color =
          properties.get_string(ElementProperty::STROKE_COLOR)) {
    result += "stroke:" + *stroke_color + ";";
  }
  if (auto fill_color = properties.get_string(ElementProperty::FILL_COLOR)) {
    result += "fill:" + *fill_color + ";";
  }
  if (auto vertical_align =
          properties.get_string(ElementProperty::VERTICAL_ALIGN)) {
    if (vertical_align == "middle") {
      result += "display:flex;justify-content:center;flex-direction: column;";
    }
    // TODO else log
  }
  return result;
}

std::string translate_frame_properties(const ElementPropertySet &properties) {
  std::string result;
  if (auto anchor_type = properties.get_string(ElementProperty::ANCHOR_TYPE);
      anchor_type && (*anchor_type == "as-char")) {
    result += "display:inline-block;";
  } else {
    result += "display:block;";
    result += "position:absolute;";
  }
  if (auto x = properties.get_string(ElementProperty::X)) {
    result += "left:" + *x + ";";
  }
  if (auto y = properties.get_string(ElementProperty::Y)) {
    result += "top:" + *y + ";";
  }
  if (auto width = properties.get_string(ElementProperty::WIDTH)) {
    result += "width:" + *width + ";";
  }
  if (auto height = properties.get_string(ElementProperty::HEIGHT)) {
    result += "height:" + *height + ";";
  }
  if (auto z_index = properties.get_string(ElementProperty::Z_INDEX)) {
    result += "z-index:" + *z_index + ";";
  }
  return result;
}

std::string translate_rect_properties(const ElementPropertySet &properties) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + properties.get_string(ElementProperty::X).value() + ";";
  result += "top:" + properties.get_string(ElementProperty::Y).value() + ";";
  result +=
      "width:" + properties.get_string(ElementProperty::WIDTH).value() + ";";
  result +=
      "height:" + properties.get_string(ElementProperty::HEIGHT).value() + ";";
  return result;
}

std::string translate_circle_properties(const ElementPropertySet &properties) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + properties.get_string(ElementProperty::X).value() + ";";
  result += "top:" + properties.get_string(ElementProperty::Y).value() + ";";
  result +=
      "width:" + properties.get_string(ElementProperty::WIDTH).value() + ";";
  result +=
      "height:" + properties.get_string(ElementProperty::HEIGHT).value() + ";";
  return result;
}

std::string
translate_custom_shape_properties(const ElementPropertySet &properties) {
  std::string result;
  result += "position:absolute;";
  if (auto x = properties.get_string(ElementProperty::X)) {
    result += "left:" + *x + ";";
  } else {
    result += "left:0;";
  }
  if (auto y = properties.get_string(ElementProperty::Y)) {
    result += "top:" + *y + ";";
  } else {
    result += "top:0;";
  }
  result +=
      "width:" + properties.get_string(ElementProperty::WIDTH).value() + ";";
  result +=
      "height:" + properties.get_string(ElementProperty::HEIGHT).value() + ";";
  return result;
}

std::string optional_style_attribute(const std::string &style) {
  if (style.empty()) {
    return "";
  }
  return " style=\"" + style + "\"";
}

void translate_text(const DocumentCursor &cursor, std::ostream &out) {
  auto properties = cursor.element_properties();

  out << common::html::escape_text(
      properties.get_string(ElementProperty::TEXT).value());
}

void translate_paragraph(DocumentCursor &cursor, std::ostream &out,
                         const HtmlConfig &config) {
  auto properties = cursor.element_properties();

  out << "<p";
  out << optional_style_attribute(translate_paragraph_style(properties));
  out << ">";
  out << "<span";
  out << optional_style_attribute(translate_text_style(properties));
  out << ">";
  translate_children(cursor, out, config);
  out << "</span>";
  out << "</p>";
}

void translate_span(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  auto properties = cursor.element_properties();

  out << "<span";
  out << optional_style_attribute(translate_text_style(properties));
  out << ">";
  translate_children(cursor, out, config);
  out << "</span>";
}

void translate_link(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  auto properties = cursor.element_properties();

  out << "<a";
  out << optional_style_attribute(translate_text_style(properties));
  out << " href=\"";
  out << properties.get_string(ElementProperty::HREF).value();
  out << "\">";
  translate_children(cursor, out, config);
  out << "</a>";
}

void translate_bookmark(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig & /*config*/) {
  auto properties = cursor.element_properties();

  out << "<a id=\"";
  out << properties.get_string(ElementProperty::HREF).value();
  out << "\"></a>";
}

void translate_list(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<ul>";
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    out << "<li>";
    translate_children(cursor, out, config);
    out << "</li>";
  });
  out << "</ul>";
}

void translate_table(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config) {
  // TODO a lot of features here should only apply to spreadsheets
  // TODO table column width does not work
  // TODO table row height does not work

  auto properties = cursor.element_properties();
  auto table = cursor.table();

  out << "<table";
  out << optional_style_attribute(translate_table_style(properties));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  auto dimensions = table.dimensions();
  std::uint32_t end_row = dimensions.rows;
  std::uint32_t end_column = dimensions.columns;
  if (config.table_limit_rows > 0) {
    end_row = config.table_limit_rows;
  }
  if (config.table_limit_columns > 0) {
    end_column = config.table_limit_columns;
  }
  /* TODO
  if (config.table_limit_by_content) {
    const auto content_bounds = element.content_bounds();
    const auto content_bounds_within = element.content_bounds(
        {config.table_limit_rows, config.table_limit_columns});
    end_row = end_row ? content_bounds_within.rows : content_bounds.rows;
    end_column =
        end_column ? content_bounds_within.columns : content_bounds.columns;
  }
   */

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    if (cursor.element_type() == ElementType::TABLE_COLUMN) {
      auto column_properties = cursor.element_properties();

      out << "<col";
      out << optional_style_attribute(
          translate_table_column_style(column_properties));
      out << ">";
    } else if (cursor.element_type() == ElementType::TABLE_ROW) {
      auto row_properties = cursor.element_properties();

      out << "<tr";
      out << optional_style_attribute(
          translate_table_row_style(row_properties));
      out << ">";

      cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
        // TODO check if cell hidden

        auto cell_properties = cursor.element_properties();
        std::uint32_t row_span =
            cell_properties.get_uint32(ElementProperty::ROW_SPAN).value_or(1);
        std::uint32_t column_span =
            cell_properties.get_uint32(ElementProperty::COLUMN_SPAN)
                .value_or(1);

        out << "<td";
        if (row_span > 1) {
          out << " rowspan=\"" << row_span << "\"";
        }
        if (column_span > 1) {
          out << " colspan=\"" << column_span << "\"";
        }
        out << optional_style_attribute(
            translate_table_cell_style(cell_properties));
        out << ">";
        translate_children(cursor, out, config);
        out << "</td>";
      });

      out << "</tr>";
    }
  });

  out << "</table>";
}

void translate_image(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig & /*config*/) {
  auto properties = cursor.element_properties();
  auto image = cursor.image();

  out << "<img style=\"position:absolute;left:0;top:0;width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (image.internal()) {
    auto image_file = image.image_file().value();

    // TODO use stream
    std::string image_data;

    try {
      // try svm
      // TODO `impl()` might be a bit dirty
      auto image_file_impl = image_file.impl();
      // TODO memory file might not be necessary; other istreams didn't support
      // `tellg`
      svm::SvmFile svm_file(
          std::make_shared<common::MemoryFile>(*image_file_impl));
      std::ostringstream svg_out;
      svm::Translator::svg(svm_file, svg_out);
      image_data = svg_out.str();
      out << "data:image/svg+xml;base64, ";
    } catch (...) {
      // else we guess that it is a usual image
      image_data = util::stream::read(*image_file.read());
      // TODO hacky - `image/jpg` works for all common image types in chrome
      out << "data:image/jpg;base64, ";
    }

    // TODO stream
    out << crypto::util::base64_encode(image_data);
  } else {
    out << properties.get_string(ElementProperty::HREF).value();
  }

  out << "\">";
}

void translate_frame(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config) {
  auto properties = cursor.element_properties();

  // TODO choosing <span> by default because it is valid inside <p>;
  // alternatives?
  const bool span = properties.get(ElementProperty::ANCHOR_TYPE).has_value();
  if (span) {
    out << "<span";
  } else {
    out << "<div";
  }

  out << optional_style_attribute(translate_frame_properties(properties));
  out << ">";
  translate_children(cursor, out, config);

  if (span) {
    out << "</span>";
  } else {
    out << "</div>";
  }
}

void translate_rect(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  auto properties = cursor.element_properties();

  out << "<div";
  out << optional_style_attribute(translate_rect_properties(properties) +
                                  translate_drawing_style(properties));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void translate_line(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig & /*config*/) {
  auto properties = cursor.element_properties();

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optional_style_attribute("z-index:-1;position:absolute;top:0;left:0;" +
                                  translate_drawing_style(properties));
  out << ">";

  out << "<line";
  out << " x1=\"" << properties.get_string(ElementProperty::X1).value()
      << "\" y1=\"" << properties.get_string(ElementProperty::Y1).value()
      << "\"";
  out << " x2=\"" << properties.get_string(ElementProperty::X2).value()
      << "\" y2=\"" << properties.get_string(ElementProperty::Y2).value()
      << "\"";
  out << " />";

  out << "</svg>";
}

void translate_circle(DocumentCursor &cursor, std::ostream &out,
                      const HtmlConfig &config) {
  auto properties = cursor.element_properties();

  out << "<div";
  out << optional_style_attribute(translate_circle_properties(properties) +
                                  translate_drawing_style(properties));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)";
  out << "</div>";
}

void translate_custom_shape(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config) {
  auto properties = cursor.element_properties();

  out << "<div";
  out << optional_style_attribute(
      translate_custom_shape_properties(properties) +
      translate_drawing_style(properties));
  out << ">";
  translate_children(cursor, out, config);
  // TODO draw shape in svg
  out << "</div>";
}

void translate_children(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig &config) {
  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    translate_element(cursor, out, config);
  });
}

void translate_element(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config) {
  if (cursor.element_type() == ElementType::TEXT) {
    translate_text(cursor, out);
  } else if (cursor.element_type() == ElementType::LINE_BREAK) {
    out << "<br>";
  } else if (cursor.element_type() == ElementType::PARAGRAPH) {
    translate_paragraph(cursor, out, config);
  } else if (cursor.element_type() == ElementType::SPAN) {
    translate_span(cursor, out, config);
  } else if (cursor.element_type() == ElementType::LINK) {
    translate_link(cursor, out, config);
  } else if (cursor.element_type() == ElementType::BOOKMARK) {
    translate_bookmark(cursor, out, config);
  } else if (cursor.element_type() == ElementType::LIST) {
    translate_list(cursor, out, config);
  } else if (cursor.element_type() == ElementType::TABLE) {
    translate_table(cursor, out, config);
  } else if (cursor.element_type() == ElementType::FRAME) {
    translate_frame(cursor, out, config);
  } else if (cursor.element_type() == ElementType::IMAGE) {
    translate_image(cursor, out, config);
  } else if (cursor.element_type() == ElementType::RECT) {
    translate_rect(cursor, out, config);
  } else if (cursor.element_type() == ElementType::LINE) {
    translate_line(cursor, out, config);
  } else if (cursor.element_type() == ElementType::CIRCLE) {
    translate_circle(cursor, out, config);
  } else if (cursor.element_type() == ElementType::CUSTOM_SHAPE) {
    translate_custom_shape(cursor, out, config);
  } else if (cursor.element_type() == ElementType::GROUP) {
    translate_children(cursor, out, config);
  } else {
    // TODO log
  }
}

void translate_text_document(DocumentCursor &cursor, std::ostream &out,
                             const HtmlConfig &config) {
  if (config.text_document_margin) {
    auto properties = cursor.element_properties();

    out << "<div";
    out << optional_style_attribute(translate_outer_page_style(properties));
    out << ">";
    out << "<div";
    out << optional_style_attribute(translate_inner_page_style(properties));
    out << ">";
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  } else {
    translate_children(cursor, out, config);
  }
}

void translate_presentation(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config) {
  // TODO indexing is kind of ugly here and duplicated
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }

    auto properties = cursor.element_properties();

    out << "<div";
    out << optional_style_attribute(translate_outer_page_style(properties));
    out << ">";
    out << "<div";
    out << optional_style_attribute(translate_inner_page_style(properties));
    out << ">";
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  });
}

void translate_spreadsheet(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }

    translate_table(cursor, out, config);
  });
}

void translate_drawing(DocumentCursor &cursor, std::ostream &out,
                       const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t i) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      return;
    }

    auto properties = cursor.element_properties();

    out << "<div";
    out << optional_style_attribute(translate_outer_page_style(properties));
    out << ">";
    out << "<div";
    out << optional_style_attribute(translate_inner_page_style(properties));
    out << ">";
    translate_children(cursor, out, config);
    out << "</div>";
    out << "</div>";
  });
}
} // namespace

HtmlConfig html::parse_config(const std::string &path) {
  HtmlConfig result;

  auto json = nlohmann::json::parse(std::ifstream(path));
  // TODO assign config values

  return result;
}

void html::translate(const Document &document,
                     const std::string & /*document_identifier*/,
                     const std::string &path, const HtmlConfig &config) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return; // TODO throw
  }

  out << common::html::doctype();
  out << "<html><head>";
  out << common::html::default_headers();
  out << "<style>";
  out << common::html::default_style();
  if (document.document_type() == DocumentType::SPREADSHEET) {
    out << common::html::default_spreadsheet_style();
  }
  out << "</style>";
  out << "</head>";

  out << "<body " << common::html::body_attributes(config) << ">";

  auto cursor = document.root_element();

  if (document.document_type() == DocumentType::TEXT) {
    translate_text_document(cursor, out, config);
  } else if (document.document_type() == DocumentType::PRESENTATION) {
    translate_presentation(cursor, out, config);
  } else if (document.document_type() == DocumentType::SPREADSHEET) {
    translate_spreadsheet(cursor, out, config);
  } else if (document.document_type() == DocumentType::DRAWING) {
    translate_drawing(cursor, out, config);
  } else {
    // TODO throw?
  }

  out << "</body>";

  out << "<script>";
  out << common::html::default_script();
  out << "</script>";
  out << "</html>";
}

void html::edit(const Document & /*document*/,
                const std::string & /*document_identifier*/,
                const std::string & /*diff*/) {
  throw UnsupportedOperation();
}

} // namespace odr
