#include <fstream>
#include <internal/abstract/document.h>
#include <internal/common/file.h>
#include <internal/common/html.h>
#include <internal/common/table_cursor.h>
#include <internal/crypto/crypto_util.h>
#include <internal/svm/svm_file.h>
#include <internal/svm/svm_to_svg.h>
#include <internal/util/stream_util.h>
#include <menu.h>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>
#include <sstream>

using namespace odr::internal;

namespace odr {

namespace {
void translate_generation(const ElementRange &siblings, std::ostream &out,
                          const HtmlConfig &config);
void translate_element(const Element &element, std::ostream &out,
                       const HtmlConfig &config);

std::string translate_paragraph_style(const PropertySet &properties) {
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

std::string translate_text_style(const PropertySet &properties) {
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

std::string translate_table_style(const PropertySet &properties) {
  std::string result;
  if (auto width = properties.get_string(ElementProperty::WIDTH); width) {
    result += "width:" + *width + ";";
  }
  return result;
}

std::string translate_table_column_style(const PropertySet &properties) {
  std::string result;
  if (auto width = properties.get_string(ElementProperty::WIDTH)) {
    result += "width:" + *width + ";";
  }
  return result;
}

std::string translate_table_row_style(const PropertySet &properties) {
  std::string result;
  // TODO that does not work with HTML; height would need to be applied to the
  // cells
  if (auto height = properties.get_string(ElementProperty::HEIGHT)) {
    result += "height:" + *height + ";";
  }
  return result;
}

std::string translate_table_cell_style(const PropertySet &properties) {
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

std::string translate_drawing_style(const Element &element) {
  std::string result;
  if (auto stroke_width = element.property(ElementProperty::STROKE_WIDTH)) {
    result += "stroke-width:" + stroke_width.get_string() + ";";
  }
  if (auto stroke_color = element.property(ElementProperty::STROKE_COLOR)) {
    result += "stroke:" + stroke_color.get_string() + ";";
  }
  if (auto fill_color = element.property(ElementProperty::FILL_COLOR)) {
    result += "fill:" + fill_color.get_string() + ";";
  }
  if (auto vertical_align = element.property(ElementProperty::VERTICAL_ALIGN)) {
    if (vertical_align.get_string() == "middle") {
      result += "display:flex;justify-content:center;flex-direction: column;";
    }
    // TODO else log
  }
  return result;
}

std::string translate_frame_properties(const Frame &element) {
  std::string result;
  if (auto anchor_type = element.anchor_type();
      anchor_type && (anchor_type.get_string() == "as-char")) {
    result += "display:inline-block;";
  }
  if (element.x() || element.y()) {
    result += "position:absolute;";
  }
  if (auto x = element.x()) {
    result += "left:" + x.get_string() + ";";
  }
  if (auto y = element.y()) {
    result += "top:" + y.get_string() + ";";
  }
  result += "width:" + element.width().get_string() + ";";
  result += "height:" + element.height().get_string() + ";";
  if (auto z_index = element.z_index()) {
    result += "z-index:" + z_index.get_string() + ";";
  }
  return result;
}

std::string translate_rect_properties(const Rect &element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x().get_string() + ";";
  result += "top:" + element.y().get_string() + ";";
  result += "width:" + element.width().get_string() + ";";
  result += "height:" + element.height().get_string() + ";";
  return result;
}

std::string translate_circle_properties(const Circle &element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x().get_string() + ";";
  result += "top:" + element.y().get_string() + ";";
  result += "width:" + element.width().get_string() + ";";
  result += "height:" + element.height().get_string() + ";";
  return result;
}

std::string translate_custom_shape_properties(const CustomShape &element) {
  std::string result;
  result += "position:absolute;";
  if (auto x = element.x()) {
    result += "left:" + x.get_string() + ";";
  } else {
    result += "left:0;";
  }
  if (auto y = element.y()) {
    result += "top:" + y.get_string() + ";";
  } else {
    result += "top:0;";
  }
  result += "width:" + element.width().get_string() + ";";
  result += "height:" + element.height().get_string() + ";";
  return result;
}

std::string optional_style_attribute(const std::string &style) {
  if (style.empty()) {
    return "";
  }
  return " style=\"" + style + "\"";
}

void translate_paragraph(const Paragraph &element, std::ostream &out,
                         const HtmlConfig &config) {
  auto properties = element.properties();

  out << "<p";
  out << optional_style_attribute(translate_paragraph_style(properties));
  out << ">";
  out << "<span";
  out << optional_style_attribute(translate_text_style(properties));
  out << ">";
  translate_generation(element.children(), out, config);
  out << "</span>";
  out << "</p>";
}

void translate_span(const Span &element, std::ostream &out,
                    const HtmlConfig &config) {
  auto properties = element.properties();

  out << "<span";
  out << optional_style_attribute(translate_text_style(properties));
  out << ">";
  translate_generation(element.children(), out, config);
  out << "</span>";
}

void translate_link(const Link &element, std::ostream &out,
                    const HtmlConfig &config) {
  auto properties = element.properties();

  out << "<a";
  out << optional_style_attribute(translate_text_style(properties));
  out << " href=\"";
  out << element.href();
  out << "\">";
  translate_generation(element.children(), out, config);
  out << "</a>";
}

void translate_bookmark(const Bookmark &element, std::ostream &out,
                        const HtmlConfig &config) {
  out << "<a id=\"";
  out << element.name();
  out << "\"></a>";
}

void translate_list(const List &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<ul>";
  for (auto &&i : element.children()) {
    out << "<li>";
    translate_generation(i.children(), out, config);
    out << "</li>";
  }
  out << "</ul>";
}

void translate_table(const Table &element, std::ostream &out,
                     const HtmlConfig &config) {
  // TODO a lot of features here should only apply to spreadsheets
  // TODO table column width does not work
  // TODO table row height does not work

  auto properties = element.properties();

  out << "<table";
  out << optional_style_attribute(translate_table_style(properties));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  std::optional<std::uint32_t> end_column;
  std::optional<std::uint32_t> end_row;
  if (config.table_limit_cols > 0) {
    end_column = config.table_limit_cols;
  }
  if (config.table_limit_rows > 0) {
    end_row = config.table_limit_rows;
  }
  if (config.table_limit_by_content) {
    const auto content_bounds = element.content_bounds();
    end_column = content_bounds.columns;
    end_row = content_bounds.rows;
  }

  common::TableCursor cursor;

  for (auto &&col : element.columns()) {
    if (end_column && (cursor.col() >= end_column)) {
      cursor.add_col();
      continue;
    }
    cursor.add_col();

    auto column_properties = col.properties();

    out << "<col";
    out << optional_style_attribute(
        translate_table_column_style(column_properties));
    out << ">";
  }

  cursor = {};

  for (auto &&row : element.rows()) {
    if (end_row && (cursor.row() >= end_row)) {
      break;
    }

    auto row_properties = row.properties();

    out << "<tr";
    out << optional_style_attribute(translate_table_row_style(row_properties));
    out << ">";

    for (auto &&cell : row.cells()) {
      if (end_column && (cursor.col() >= end_column)) {
        break;
      }

      auto cell_properties = cell.properties();

      out << "<td";
      if (auto column_span = cell.column_span(); column_span > 1) {
        out << " colspan=\"" << column_span << "\"";
      }
      if (auto row_span = cell.row_span(); row_span > 1) {
        out << " rowspan=\"" << row_span << "\"";
      }
      out << optional_style_attribute(
          translate_table_cell_style(cell_properties));
      out << ">";
      translate_generation(cell.children(), out, config);
      out << "</td>";

      cursor.add_cell(cell.column_span(), cell.row_span());
    }

    out << "</tr>";

    cursor.add_row();
  }

  out << "</table>";
}

void translate_image(const Image &element, std::ostream &out,
                     const HtmlConfig &config) {
  out << "<img style=\"position:absolute;left:0;top:0;width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (element.internal()) {
    auto image_file = element.image_file();

    // TODO use stream
    std::string image;

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
      image = svg_out.str();
      out << "data:image/svg+xml;base64, ";
    } catch (...) {
      // else we guess that it is a usual image
      image = util::stream::read(*image_file.read());
      // TODO hacky - `image/jpg` works for all common image types in chrome
      out << "data:image/jpg;base64, ";
    }

    // TODO stream
    out << crypto::util::base64_encode(image);
  } else {
    out << element.href().get_string();
  }

  out << "\">";
}

void translate_frame(const Frame &element, std::ostream &out,
                     const HtmlConfig &config) {
  if (auto anchor_type = element.anchor_type();
      anchor_type && (anchor_type.get_string() == "as-char")) {
    out << "<span";
  } else {
    out << "<div";
  }

  out << optional_style_attribute(translate_frame_properties(element));
  out << ">";
  translate_generation(element.children(), out, config);
  out << "</div>";
}

void translate_rect(const Rect &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<div";
  out << optional_style_attribute(translate_rect_properties(element) +
                                  translate_drawing_style(element));
  out << ">";
  translate_generation(element.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void translate_line(const Line &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optional_style_attribute("z-index:-1;position:absolute;top:0;left:0;" +
                                  translate_drawing_style(element));
  out << ">";

  out << "<line";
  out << " x1=\"" << element.x1().get_string() << "\" y1=\""
      << element.y1().get_string() << "\"";
  out << " x2=\"" << element.x2().get_string() << "\" y2=\""
      << element.y2().get_string() << "\"";
  out << " />";

  out << "</svg>";
}

void translate_circle(const Circle &element, std::ostream &out,
                      const HtmlConfig &config) {
  out << "<div";
  out << optional_style_attribute(translate_circle_properties(element) +
                                  translate_drawing_style(element));
  out << ">";
  translate_generation(element.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)";
  out << "</div>";
}

void translate_custom_shape(const CustomShape &element, std::ostream &out,
                            const HtmlConfig &config) {
  out << "<div";
  out << optional_style_attribute(translate_custom_shape_properties(element) +
                                  translate_drawing_style(element));
  out << ">";
  translate_generation(element.children(), out, config);
  // TODO draw shape in svg
  out << "</div>";
}

void translate_generation(const ElementRange &siblings, std::ostream &out,
                          const HtmlConfig &config) {
  for (auto &&e : siblings) {
    translate_element(e, out, config);
  }
}

void translate_element(const Element &element, std::ostream &out,
                       const HtmlConfig &config) {
  if (element.type() == ElementType::TEXT) {
    // TODO handle whitespace collapse
    out << common::html::escape_text(element.text().string());
  } else if (element.type() == ElementType::LINE_BREAK) {
    out << "<br>";
  } else if (element.type() == ElementType::PARAGRAPH) {
    translate_paragraph(element.paragraph(), out, config);
  } else if (element.type() == ElementType::SPAN) {
    translate_span(element.span(), out, config);
  } else if (element.type() == ElementType::LINK) {
    translate_link(element.link(), out, config);
  } else if (element.type() == ElementType::BOOKMARK) {
    translate_bookmark(element.bookmark(), out, config);
  } else if (element.type() == ElementType::LIST) {
    translate_list(element.list(), out, config);
  } else if (element.type() == ElementType::TABLE) {
    translate_table(element.table(), out, config);
  } else if (element.type() == ElementType::FRAME) {
    translate_frame(element.frame(), out, config);
  } else if (element.type() == ElementType::IMAGE) {
    translate_image(element.image(), out, config);
  } else if (element.type() == ElementType::RECT) {
    translate_rect(element.rect(), out, config);
  } else if (element.type() == ElementType::LINE) {
    translate_line(element.line(), out, config);
  } else if (element.type() == ElementType::CIRCLE) {
    translate_circle(element.circle(), out, config);
  } else if (element.type() == ElementType::CUSTOM_SHAPE) {
    translate_custom_shape(element.custom_shape(), out, config);
  } else {
    // TODO log
  }
}

void translate_text_document(const TextDocument &document, std::ostream &out,
                             const HtmlConfig &config) {
  const auto root = document.root();

  if (config.text_document_margin) {
    // TODO check if props are available
    const std::string outer_style =
        "width:" + root.property(ElementProperty::WIDTH).get_string() + ";";
    const std::string inner_style =
        "margin-top:" +
        root.property(ElementProperty::MARGIN_TOP).get_string() + ";" +
        "margin-left:" +
        root.property(ElementProperty::MARGIN_LEFT).get_string() + ";" +
        "margin-bottom:" +
        root.property(ElementProperty::MARGIN_BOTTOM).get_string() + ";" +
        "margin-right:" +
        root.property(ElementProperty::MARGIN_RIGHT).get_string() + ";";

    out << R"(<div style=")" + outer_style + "\">";
    out << R"(<div style=")" + inner_style + "\">";
    translate_generation(document.root().children(), out, config);
    out << "</div>";
    out << "</div>";
  } else {
    translate_generation(document.root().children(), out, config);
  }
}

void translate_presentation(const Presentation &document, std::ostream &out,
                            const HtmlConfig &config) {
  // TODO indexing is kind of ugly here and duplicated
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  std::uint32_t i = 0;
  for (auto &&slide : document.slides()) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      ++i;
      continue;
    }
    ++i;

    const std::string outer_style =
        "width:" + slide.property(ElementProperty::WIDTH).get_string() + ";" +
        "height:" + slide.property(ElementProperty::HEIGHT).get_string() + ";";
    const std::string inner_style =
        "margin-top:" +
        slide.property(ElementProperty::MARGIN_TOP).get_string() + ";" +
        "margin-left:" +
        slide.property(ElementProperty::MARGIN_LEFT).get_string() + ";" +
        "margin-bottom:" +
        slide.property(ElementProperty::MARGIN_BOTTOM).get_string() + ";" +
        "margin-right:" +
        slide.property(ElementProperty::MARGIN_RIGHT).get_string() + ";";

    out << R"(<div style=")" + outer_style + "\">";
    out << R"(<div style=")" + inner_style + "\">";
    translate_generation(slide.children(), out, config);
    out << "</div>";
    out << "</div>";
  }
}

void translate_spreadsheet(const Spreadsheet &document, std::ostream &out,
                           const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  std::uint32_t i = 0;
  for (auto &&sheet : document.sheets()) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      ++i;
      continue;
    }
    ++i;

    translate_table(sheet.table(), out, config);
  }
}

void translate_drawing(const Drawing &document, std::ostream &out,
                       const HtmlConfig &config) {
  std::uint32_t begin_entry = config.entry_offset;
  std::optional<std::uint32_t> end_entry;

  if (config.entry_count > 0) {
    end_entry = begin_entry + config.entry_count;
  }

  std::uint32_t i = 0;
  for (auto &&page : document.pages()) {
    if ((i < begin_entry) || (end_entry && (i >= end_entry))) {
      ++i;
      continue;
    }
    ++i;

    const std::string outer_style =
        "width:" + page.property(ElementProperty::WIDTH).get_string() + ";" +
        "height:" + page.property(ElementProperty::HEIGHT).get_string() + ";";
    const std::string inner_style =
        "margin-top:" +
        page.property(ElementProperty::MARGIN_TOP).get_string() + ";" +
        "margin-left:" +
        page.property(ElementProperty::MARGIN_LEFT).get_string() + ";" +
        "margin-bottom:" +
        page.property(ElementProperty::MARGIN_BOTTOM).get_string() + ";" +
        "margin-right:" +
        page.property(ElementProperty::MARGIN_RIGHT).get_string() + ";";

    out << R"(<div style=")" + outer_style + "\">";
    out << R"(<div style=")" + inner_style + "\">";
    translate_generation(page.children(), out, config);
    out << "</div>";
    out << "</div>";
  }
}
} // namespace

HtmlConfig html::parse_config(const std::string &path) {
  HtmlConfig result;

  auto json = nlohmann::json::parse(std::ifstream(path));
  // TODO assign config values

  return result;
}

void html::translate(const Document &document,
                     const std::string &document_identifier,
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

  if (document.document_type() == DocumentType::TEXT) {
    translate_text_document(document.text_document(), out, config);
  } else if (document.document_type() == DocumentType::PRESENTATION) {
    translate_presentation(document.presentation(), out, config);
  } else if (document.document_type() == DocumentType::SPREADSHEET) {
    translate_spreadsheet(document.spreadsheet(), out, config);
  } else if (document.document_type() == DocumentType::DRAWING) {
    translate_drawing(document.drawing(), out, config);
  } else {
    // TODO throw?
  }

  out << "</body>";

  out << "<script>";
  out << common::html::default_script();
  out << "</script>";
  out << "</html>";
}

void html::edit(const Document &document,
                const std::string &document_identifier,
                const std::string &diff) {
  throw UnsupportedOperation();
}

} // namespace odr
