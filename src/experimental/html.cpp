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
#include <odr/experimental/document.h>
#include <odr/experimental/document_elements.h>
#include <odr/experimental/document_style.h>
#include <odr/experimental/document_type.h>
#include <odr/experimental/element_type.h>
#include <odr/experimental/file.h>
#include <odr/experimental/html.h>
#include <odr/experimental/property.h>
#include <odr/experimental/table_dimensions.h>
#include <odr/html_config.h>
#include <sstream>

using namespace odr::internal;

namespace odr::experimental {

namespace {
void translate_generation(const ElementRange &siblings, std::ostream &out,
                          const HtmlConfig &config);
void translate_element(const Element &element, std::ostream &out,
                       const HtmlConfig &config);

std::string translate_paragraph_style(const ParagraphStyle &style) {
  std::string result;
  if (style.text_align()) {
    result += "text-align:" + *style.text_align() + ";";
  }
  if (style.margin_top()) {
    result += "margin-top:" + *style.margin_top() + ";";
  }
  if (style.margin_bottom()) {
    result += "margin-bottom:" + *style.margin_bottom() + ";";
  }
  if (style.margin_left()) {
    result += "margin-left:" + *style.margin_left() + ";";
  }
  if (style.margin_right()) {
    result += "margin-right:" + *style.margin_right() + ";";
  }
  return result;
}

std::string translate_text_style(const TextStyle &style) {
  std::string result;
  if (style.font_name()) {
    result += "font-family:" + *style.font_name() + ";";
  }
  if (style.font_size()) {
    result += "font-size:" + *style.font_size() + ";";
  }
  if (style.font_weight()) {
    result += "font-weight:" + *style.font_weight() + ";";
  }
  if (style.font_style()) {
    result += "font-style:" + *style.font_style() + ";";
  }
  if (style.font_color()) {
    result += "color:" + *style.font_color() + ";";
  }
  if (style.background_color()) {
    result += "background-color:" + *style.background_color() + ";";
  }
  return result;
}

std::string translate_table_style(const TableStyle &style) {
  std::string result;
  if (style.width()) {
    result += "width:" + *style.width() + ";";
  }
  return result;
}

std::string translate_table_column_style(const TableColumnStyle &style) {
  std::string result;
  if (style.width()) {
    result += "width:" + *style.width() + ";";
  }
  return result;
}

std::string translate_table_cell_style(const TableCellStyle &style) {
  std::string result;
  if (style.padding_top()) {
    result += "padding-top:" + *style.padding_top() + ";";
  }
  if (style.padding_bottom()) {
    result += "padding-bottom:" + *style.padding_bottom() + ";";
  }
  if (style.padding_left()) {
    result += "padding-left:" + *style.padding_left() + ";";
  }
  if (style.padding_right()) {
    result += "padding-right:" + *style.padding_right() + ";";
  }
  if (style.border_top()) {
    result += "border-top:" + *style.border_top() + ";";
  }
  if (style.border_bottom()) {
    result += "border-bottom:" + *style.border_bottom() + ";";
  }
  if (style.border_left()) {
    result += "border-left:" + *style.border_left() + ";";
  }
  if (style.border_right()) {
    result += "border-right:" + *style.border_right() + ";";
  }
  return result;
}

std::string translate_drawing_style(const DrawingStyle &style) {
  std::string result;
  if (style.stroke_width()) {
    result += "stroke-width:" + *style.stroke_width() + ";";
  }
  if (style.stroke_color()) {
    result += "stroke:" + *style.stroke_color() + ";";
  }
  if (style.fill_color()) {
    result += "fill:" + *style.fill_color() + ";";
  }
  if (style.vertical_align()) {
    if (*style.vertical_align() == "middle") {
      result += "display:flex;justify-content:center;flex-direction: column;";
    }
    // TODO else log
  }
  return result;
}

std::string translate_frame_properties(const FrameElement &properties) {
  std::string result;
  result += "width:" + *properties.width() + ";";
  result += "height:" + *properties.height() + ";";
  result += "z-index:" + *properties.z_index() + ";";
  return result;
}

std::string translate_rect_properties(const RectElement &element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x() + ";";
  result += "top:" + element.y() + ";";
  result += "width:" + element.width() + ";";
  result += "height:" + element.height() + ";";
  return result;
}

std::string translate_circle_properties(const CircleElement &element) {
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

void translate_paragraph(const ParagraphElement &element, std::ostream &out,
                         const HtmlConfig &config) {
  out << "<p";
  out << optional_style_attribute(
      translate_paragraph_style(element.paragraph_style()));
  out << ">";
  out << "<span";
  out << optional_style_attribute(translate_text_style(element.text_style()));
  out << ">";
  translate_generation(element.children(), out, config);
  out << "</span>";
  out << "</p>";
}

void translate_span(const SpanElement &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<span";
  out << optional_style_attribute(translate_text_style(element.text_style()));
  out << ">";
  translate_generation(element.children(), out, config);
  out << "</span>";
}

void translate_link(const LinkElement &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<a";
  out << optional_style_attribute(translate_text_style(element.text_style()));
  out << " href=\"";
  out << element.href();
  out << "\">";
  translate_generation(element.children(), out, config);
  out << "</a>";
}

void translate_bookmark(const BookmarkElement &element, std::ostream &out,
                        const HtmlConfig &config) {
  out << "<a id=\"";
  out << element.name();
  out << "\"></a>";
}

void translate_list(const ListElement &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<ul>";
  for (auto &&i : element.children()) {
    out << "<li>";
    translate_generation(i.children(), out, config);
    out << "</li>";
  }
  out << "</ul>";
}

void translate_table(const TableElement &element, std::ostream &out,
                     const HtmlConfig &config) {
  out << "<table";
  out << optional_style_attribute(translate_table_style(element.table_style()));
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
    out << optional_style_attribute(
        translate_table_column_style(col.table_column_style()));
    out << ">";
  }

  std::uint32_t row_index = 0;
  for (auto &&row : element.rows()) {
    if (end_row && (row_index >= end_row)) {
      ++row_index;
      continue;
    }
    ++row_index;

    out << "<tr>";
    std::uint32_t column_index = 0;
    for (auto &&cell : row.cells()) {
      if (end_column && (column_index >= end_column)) {
        ++column_index;
        continue;
      }
      ++column_index;

      out << "<td";
      out << optional_style_attribute(
          translate_table_cell_style(cell.table_cell_style()));
      out << ">";
      translate_generation(cell.children(), out, config);
      out << "</td>";
    }
    out << "</tr>";
  }

  out << "</table>";
}

void translate_image(const ImageElement &element, std::ostream &out,
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

void translate_frame(const FrameElement &element, std::ostream &out,
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

void translate_rect(const RectElement &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<div";
  out << optional_style_attribute(
      translate_rect_properties(element) +
      translate_drawing_style(element.drawing_style()));
  out << ">";
  translate_generation(element.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void translate_line(const LineElement &element, std::ostream &out,
                    const HtmlConfig &config) {
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optional_style_attribute(
      "z-index:-1;position:absolute;top:0;left:0;" +
      translate_drawing_style(element.drawing_style()));
  out << ">";

  out << "<line";
  out << " x1=\"" << element.x1() << "\" y1=\"" << element.y1() << "\"";
  out << " x2=\"" << element.x2() << "\" y2=\"" << element.y2() << "\"";
  out << " />";

  out << "</svg>";
}

void translate_circle(const CircleElement &element, std::ostream &out,
                      const HtmlConfig &config) {
  out << "<div";
  out << optional_style_attribute(
      translate_circle_properties(element) +
      translate_drawing_style(element.drawing_style()));
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
    out << common::Html::escape_text(element.text().string());
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
  const auto page_style = document.page_style();

  if (config.text_document_margin && page_style) {
    const std::string outer_style = "width:" + *page_style.width() + ";";
    const std::string inner_style =
        "margin-top:" + *page_style.margin_top() + ";" +
        "margin-left:" + *page_style.margin_left() + ";" +
        "margin-bottom:" + *page_style.margin_bottom() + ";" +
        "margin-right:" + *page_style.margin_right() + ";";

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

    const auto page_style = slide.page_style();

    const std::string outer_style = "width:" + *page_style.width() + ";" +
                                    "height:" + *page_style.height() + ";";
    const std::string inner_style =
        "margin-top:" + *page_style.margin_top() + ";" +
        "margin-left:" + *page_style.margin_left() + ";" +
        "margin-bottom:" + *page_style.margin_bottom() + ";" +
        "margin-right:" + *page_style.margin_right() + ";";

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

    const auto page_style = page.page_style();

    const std::string outer_style = "width:" + *page_style.width() + ";" +
                                    "height:" + *page_style.height() + ";";
    const std::string inner_style =
        "margin-top:" + *page_style.margin_top() + ";" +
        "margin-left:" + *page_style.margin_left() + ";" +
        "margin-bottom:" + *page_style.margin_bottom() + ";" +
        "margin-right:" + *page_style.margin_right() + ";";

    out << R"(<div style=")" + outer_style + "\">";
    out << R"(<div style=")" + inner_style + "\">";
    translate_generation(page.children(), out, config);
    out << "</div>";
    out << "</div>";
  }
}
} // namespace

HtmlConfig Html::parse_config(const std::string &path) {
  HtmlConfig result;

  auto json = nlohmann::json::parse(std::ifstream(path));
  // TODO assign config values

  return result;
}

void Html::translate(const Document &document,
                     const std::string &document_identifier,
                     const std::string &path, const HtmlConfig &config) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return; // TODO throw
  }

  out << common::Html::doctype();
  out << "<html><head>";
  out << common::Html::default_headers();
  out << "<style>";
  out << common::Html::default_style();
  out << "</style>";
  out << "</head>";

  out << "<body " << common::Html::body_attributes(config) << ">";

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
  out << common::Html::default_script();
  out << "</script>";
  out << "</html>";
}

void Html::edit(const Document &document,
                const std::string &document_identifier,
                const std::string &diff) {
  throw UnsupportedOperation();
}

} // namespace odr::experimental
