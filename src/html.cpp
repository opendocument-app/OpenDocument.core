#include <fstream>
#include <internal/abstract/document.h>
#include <internal/common/html.h>
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
void translate_generation(const ElementRange &siblings, std::ostream &out,
                          const HtmlConfig &config);
void translate_element(const Element &element, std::ostream &out,
                       const HtmlConfig &config);

std::string translate_paragraph_style(const Element &element) {
  std::string result;
  if (auto text_align = element.property(ElementProperty::TEXT_ALIGN);
      text_align) {
    result += "text-align:" + text_align.get_string() + ";";
  }
  if (auto margin_top = element.property(ElementProperty::MARGIN_TOP);
      margin_top) {
    result += "margin-top:" + margin_top.get_string() + ";";
  }
  if (auto margin_bottom = element.property(ElementProperty::MARGIN_BOTTOM);
      margin_bottom) {
    result += "margin-bottom:" + margin_bottom.get_string() + ";";
  }
  if (auto margin_left = element.property(ElementProperty::MARGIN_LEFT);
      margin_left) {
    result += "margin-left:" + margin_left.get_string() + ";";
  }
  if (auto margin_right = element.property(ElementProperty::MARGIN_RIGHT);
      margin_right) {
    result += "margin-right:" + margin_right.get_string() + ";";
  }
  return result;
}

std::string translate_text_style(const Element &element) {
  std::string result;
  if (auto font_name = element.property(ElementProperty::FONT_NAME);
      font_name) {
    result += "font-family:" + font_name.get_string() + ";";
  }
  if (auto font_size = element.property(ElementProperty::FONT_SIZE);
      font_size) {
    result += "font-size:" + font_size.get_string() + ";";
  }
  if (auto font_weight = element.property(ElementProperty::FONT_WEIGHT);
      font_weight) {
    result += "font-weight:" + font_weight.get_string() + ";";
  }
  if (auto font_style = element.property(ElementProperty::FONT_STYLE);
      font_style) {
    result += "font-style:" + font_style.get_string() + ";";
  }
  if (auto font_color = element.property(ElementProperty::FONT_COLOR);
      font_color) {
    result += "color:" + font_color.get_string() + ";";
  }
  if (auto background_color =
          element.property(ElementProperty::BACKGROUND_COLOR);
      background_color) {
    result += "background-color:" + background_color.get_string() + ";";
  }
  return result;
}

std::string translate_table_style(const Element &element) {
  std::string result;
  if (auto width = element.property(ElementProperty::WIDTH); width) {
    result += "width:" + width.get_string() + ";";
  }
  return result;
}

std::string translate_table_column_style(const TableColumn &element) {
  std::string result;
  if (auto width = element.property(ElementProperty::WIDTH); width) {
    result += "width:" + width.get_string() + ";";
  }
  return result;
}

std::string translate_table_cell_style(const TableCell &element) {
  std::string result;
  if (auto padding_top = element.property(ElementProperty::PADDING_TOP);
      padding_top) {
    result += "padding-top:" + padding_top.get_string() + ";";
  }
  if (auto padding_bottom = element.property(ElementProperty::PADDING_BOTTOM);
      padding_bottom) {
    result += "padding-bottom:" + padding_bottom.get_string() + ";";
  }
  if (auto padding_left = element.property(ElementProperty::PADDING_LEFT);
      padding_left) {
    result += "padding-left:" + padding_left.get_string() + ";";
  }
  if (auto padding_right = element.property(ElementProperty::PADDING_RIGHT);
      padding_right) {
    result += "padding-right:" + padding_right.get_string() + ";";
  }
  if (auto border_top = element.property(ElementProperty::BORDER_TOP);
      border_top) {
    result += "border-top:" + border_top.get_string() + ";";
  }
  if (auto border_bottom = element.property(ElementProperty::BORDER_BOTTOM);
      border_bottom) {
    result += "border-bottom:" + border_bottom.get_string() + ";";
  }
  if (auto border_left = element.property(ElementProperty::BORDER_LEFT);
      border_left) {
    result += "border-left:" + border_left.get_string() + ";";
  }
  if (auto border_right = element.property(ElementProperty::BORDER_RIGHT);
      border_right) {
    result += "border-right:" + border_right.get_string() + ";";
  }
  return result;
}

std::string translate_drawing_style(const Element &element) {
  std::string result;
  if (auto stroke_width = element.property(ElementProperty::STROKE_WIDTH);
      stroke_width) {
    result += "stroke-width:" + stroke_width.get_string() + ";";
  }
  if (auto stroke_color = element.property(ElementProperty::STROKE_COLOR);
      stroke_color) {
    result += "stroke:" + stroke_color.get_string() + ";";
  }
  if (auto fill_color = element.property(ElementProperty::FILL_COLOR);
      fill_color) {
    result += "fill:" + fill_color.get_string() + ";";
  }
  if (auto vertical_align = element.property(ElementProperty::VERTICAL_ALIGN);
      vertical_align) {
    if (vertical_align.get_string() == "middle") {
      result += "display:flex;justify-content:center;flex-direction: column;";
    }
    // TODO else log
  }
  return result;
}

std::string translate_frame_properties(const Frame &properties) {
  std::string result;
  result += "width:" + properties.width() + ";";
  result += "height:" + properties.height() + ";";
  result += "z-index:" + properties.z_index() + ";";
  return result;
}

std::string translate_rect_properties(const Rect &element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x() + ";";
  result += "top:" + element.y() + ";";
  result += "width:" + element.width() + ";";
  result += "height:" + element.height() + ";";
  return result;
}

std::string translate_circle_properties(const Circle &element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x() + ";";
  result += "top:" + element.y() + ";";
  result += "width:" + element.width() + ";";
  result += "height:" + element.height() + ";";
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
  out << "<p";
  out << optional_style_attribute(translate_paragraph_style(element));
  out << ">";
  out << "<span";
  out << optional_style_attribute(translate_text_style(element));
  out << ">";
  translate_generation(element.children(), out, config);
  out << "</span>";
  out << "</p>";
}

void translate_span(const Span &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<span";
  out << optional_style_attribute(translate_text_style(element));
  out << ">";
  translate_generation(element.children(), out, config);
  out << "</span>";
}

void translate_link(const Link &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<a";
  out << optional_style_attribute(translate_text_style(element));
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
  out << "<table";
  out << optional_style_attribute(translate_table_style(element));
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

  if (config.table_limit_by_dimensions) {
    const auto dimensions = element.dimensions();
    end_column = dimensions.columns;
    end_row = dimensions.rows;
  }

  std::uint32_t column_index = 0;
  for (auto &&col : element.columns()) {
    if (end_column && (column_index >= end_column)) {
      ++column_index;
      continue;
    }
    ++column_index;

    out << "<col";
    out << optional_style_attribute(translate_table_column_style(col));
    out << ">";
  }

  std::uint32_t row_index = 0;
  for (auto &&row : element.rows()) {
    if (end_row && (row_index >= end_row)) {
      break;
    }
    ++row_index;

    out << "<tr>";
    std::uint32_t column_index = 0;
    for (auto &&cell : row.cells()) {
      if (end_column && (column_index >= end_column)) {
        break;
      }
      ++column_index;

      out << "<td";
      out << optional_style_attribute(translate_table_cell_style(cell));
      out << ">";
      translate_generation(cell.children(), out, config);
      out << "</td>";
    }
    out << "</tr>";
  }

  out << "</table>";
}

void translate_image(const Image &element, std::ostream &out,
                     const HtmlConfig &config) {
  out << "<img style=\"width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (element.internal()) {
    auto image_file = element.image_file();

    // TODO use stream
    std::string image;

    try {
      // try svm
      // TODO `impl()` might be a bit dirty
      svm::SvmFile svm_file(image_file.impl());
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
    out << element.href();
  }

  out << "\">";
}

void translate_frame(const Frame &element, std::ostream &out,
                     const HtmlConfig &config) {
  out << "<div";
  out << optional_style_attribute(translate_frame_properties(element));
  out << ">";

  for (auto &&e : element.children()) {
    if (e.type() == ElementType::IMAGE) {
      translate_image(e.image(), out, config);
    }
  }

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
  out << " x1=\"" << element.x1() << "\" y1=\"" << element.y1() << "\"";
  out << " x2=\"" << element.x2() << "\" y2=\"" << element.y2() << "\"";
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
  } else if (element.type() == ElementType::RECT) {
    translate_rect(element.rect(), out, config);
  } else if (element.type() == ElementType::LINE) {
    translate_line(element.line(), out, config);
  } else if (element.type() == ElementType::CIRCLE) {
    translate_circle(element.circle(), out, config);
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
