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

std::string translate_outer_page_style(const Style::PageLayout &page_layout) {
  std::string result;
  if (auto width = page_layout.width().value()) {
    result += "width:" + *width + ";";
  }
  if (auto height = page_layout.height().value()) {
    result += "height:" + *height + ";";
  }
  return result;
}

std::string translate_inner_page_style(const Style::PageLayout &page_layout) {
  std::string result;
  if (auto margin_top = page_layout.margin().top().value()) {
    result += "margin-top:" + *margin_top + ";";
  }
  if (auto margin_left = page_layout.margin().left().value()) {
    result += "margin-left:" + *margin_left + ";";
  }
  if (auto margin_bottom = page_layout.margin().bottom().value()) {
    result += "margin-bottom:" + *margin_bottom + ";";
  }
  if (auto margin_right = page_layout.margin().right().value()) {
    result += "margin-right:" + *margin_right + ";";
  }
  return result;
}

std::string translate_paragraph_style(const Style::Paragraph &paragraph) {
  std::string result;
  if (auto text_align = paragraph.text_align().value()) {
    result += "text-align:" + *text_align + ";";
  }
  if (auto margin_top = paragraph.margin().top().value()) {
    result += "margin-top:" + *margin_top + ";";
  }
  if (auto margin_left = paragraph.margin().left().value()) {
    result += "margin-left:" + *margin_left + ";";
  }
  if (auto margin_bottom = paragraph.margin().bottom().value()) {
    result += "margin-bottom:" + *margin_bottom + ";";
  }
  if (auto margin_right = paragraph.margin().right().value()) {
    result += "margin-right:" + *margin_right + ";";
  }
  return result;
}

std::string translate_text_style(const Style::Text &text) {
  std::string result;
  if (auto font_name = text.font_name().value()) {
    result += "font-family:" + *font_name + ";";
  }
  if (auto font_size = text.font_size().value()) {
    result += "font-size:" + *font_size + ";";
  }
  if (auto font_weight = text.font_weight().value()) {
    result += "font-weight:" + *font_weight + ";";
  }
  if (auto font_style = text.font_style().value()) {
    result += "font-style:" + *font_style + ";";
  }
  if (auto underline = text.font_underline().value();
      underline && underline == "solid") {
    result += "text-decoration:underline;";
  }
  if (auto strikethrough = text.font_line_through().value();
      strikethrough && strikethrough == "solid") {
    result += "text-decoration:line-through;";
  }
  if (auto font_shadow = text.font_shadow().value()) {
    result += "text-shadow:" + *font_shadow + ";";
  }
  if (auto font_color = text.font_color().value()) {
    result += "color:" + *font_color + ";";
  }
  if (auto background_color = text.background_color().value();
      background_color && background_color != "transparent") {
    result += "background-color:" + *background_color + ";";
  }
  return result;
}

std::string translate_table_style(const Style::Table &table) {
  std::string result;
  if (auto width = table.width().value()) {
    result += "width:" + *width + ";";
  }
  return result;
}

std::string
translate_table_column_style(const Style::TableColumn &table_column) {
  std::string result;
  if (auto width = table_column.width().value()) {
    result += "width:" + *width + ";";
  }
  return result;
}

std::string translate_table_row_style(const Style::TableRow &table_row) {
  std::string result;
  // TODO that does not work with HTML; height would need to be applied to the
  // cells
  if (auto height = table_row.height().value()) {
    result += "height:" + *height + ";";
  }
  return result;
}

std::string translate_table_cell_style(const Style &style) {
  std::string result;

  result += translate_text_style(style.text());
  result += translate_paragraph_style(style.paragraph());

  // TODO check value type for alignment

  if (auto vertical_align = style.table_cell().vertical_align().value()) {
    // TODO
  }
  if (auto background_color = style.table_cell().background_color().value();
      background_color && *background_color != "transparent") {
    result += "background-color:" + *background_color + ";";
  }
  if (auto padding_top = style.table_cell().padding().top().value()) {
    result += "padding-top:" + *padding_top + ";";
  }
  if (auto padding_left = style.table_cell().padding().left().value()) {
    result += "padding-left:" + *padding_left + ";";
  }
  if (auto padding_bottom = style.table_cell().padding().bottom().value()) {
    result += "padding-bottom:" + *padding_bottom + ";";
  }
  if (auto padding_right = style.table_cell().padding().right().value()) {
    result += "padding-right:" + *padding_right + ";";
  }
  if (auto border_top = style.table_cell().border().top().value()) {
    result += "border-top:" + *border_top + ";";
  }
  if (auto border_left = style.table_cell().border().left().value()) {
    result += "border-left:" + *border_left + ";";
  }
  if (auto border_bottom = style.table_cell().border().bottom().value()) {
    result += "border-bottom:" + *border_bottom + ";";
  }
  if (auto border_right = style.table_cell().border().right().value()) {
    result += "border-right:" + *border_right + ";";
  }
  return result;
}

std::string translate_drawing_style(const Style::Graphic &graphic) {
  std::string result;
  if (auto stroke_width = graphic.stroke_width().value()) {
    result += "stroke-width:" + *stroke_width + ";";
  }
  if (auto stroke_color = graphic.stroke_color().value()) {
    result += "stroke:" + *stroke_color + ";";
  }
  if (auto fill_color = graphic.fill_color().value()) {
    result += "fill:" + *fill_color + ";";
  }
  if (auto vertical_align = graphic.vertical_align().value()) {
    if (vertical_align == "middle") {
      result += "display:flex;justify-content:center;flex-direction:column;";
    }
    // TODO else log
  }
  return result;
}

std::string translate_frame_properties(const Element::Frame &frame) {
  std::string result;
  if (auto anchor_type = frame.anchor_type();
      anchor_type && (*anchor_type == "as-char")) {
    result += "display:inline-block;";
  } else {
    result += "display:block;";
    result += "position:absolute;";
  }
  if (auto x = frame.x()) {
    result += "left:" + *x + ";";
  }
  if (auto y = frame.y()) {
    result += "top:" + *y + ";";
  }
  if (auto width = frame.width()) {
    result += "width:" + *width + ";";
  }
  if (auto height = frame.height()) {
    result += "height:" + *height + ";";
  }
  if (auto z_index = frame.z_index()) {
    result += "z-index:" + *z_index + ";";
  }
  return result;
}

std::string translate_rect_properties(const Element::Rect &rect) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + rect.x() + ";";
  result += "top:" + rect.y() + ";";
  result += "width:" + rect.width() + ";";
  result += "height:" + rect.height() + ";";
  return result;
}

std::string translate_circle_properties(const Element::Circle &circle) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + circle.x() + ";";
  result += "top:" + circle.y() + ";";
  result += "width:" + circle.width() + ";";
  result += "height:" + circle.height() + ";";
  return result;
}

std::string
translate_custom_shape_properties(const Element::CustomShape &custom_shape) {
  std::string result;
  result += "position:absolute;";
  if (auto x = custom_shape.x()) {
    result += "left:" + *x + ";";
  } else {
    result += "left:0;";
  }
  if (auto y = custom_shape.y()) {
    result += "top:" + *y + ";";
  } else {
    result += "top:0;";
  }
  result += "width:" + custom_shape.width() + ";";
  result += "height:" + custom_shape.height() + ";";
  return result;
}

std::string optional_style_attribute(const std::string &style) {
  if (style.empty()) {
    return "";
  }
  return " style=\"" + style + "\"";
}

void translate_text(const DocumentCursor &cursor, std::ostream &out) {
  out << common::html::escape_text(cursor.element().text().value());
}

void translate_paragraph(DocumentCursor &cursor, std::ostream &out,
                         const HtmlConfig &config) {
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<p";
  out << optional_style_attribute(translate_paragraph_style(style.paragraph()));
  out << ">";
  out << "<span";
  out << optional_style_attribute(translate_text_style(style.text()));
  out << ">";
  translate_children(cursor, out, config);
  out << "</span>";
  out << "</p>";
}

void translate_span(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<span";
  out << optional_style_attribute(translate_text_style(style.text()));
  out << ">";
  translate_children(cursor, out, config);
  out << "</span>";
}

void translate_link(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig &config) {
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<a";
  out << optional_style_attribute(translate_text_style(style.text()));
  out << " href=\"";
  out << cursor.element().link().href();
  out << "\">";
  translate_children(cursor, out, config);
  out << "</a>";
}

void translate_bookmark(DocumentCursor &cursor, std::ostream &out,
                        const HtmlConfig & /*config*/) {
  out << "<a id=\"";
  out << cursor.element().bookmark().name();
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
  // TODO table column width does not work
  // TODO table row height does not work
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<table";
  out << optional_style_attribute(translate_table_style(style.table()));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  cursor.for_each_column([&](DocumentCursor &cursor, const std::uint32_t) {
    out << "<col";
    out << optional_style_attribute(
        translate_table_column_style(style.table_column()));
    out << ">";

    return true;
  });

  cursor.for_each_row([&](DocumentCursor &cursor, const std::uint32_t) {
    auto style = cursor.element().style(StyleContext::style_tree);

    out << "<tr";
    out << optional_style_attribute(
        translate_table_row_style(style.table_row()));
    out << ">";

    cursor.for_each_cell([&](DocumentCursor &cursor, const std::uint32_t) {
      auto style = cursor.element().style(StyleContext::style_tree);
      auto cell_span = cursor.element().table_cell().span();

      out << "<td";
      if (cell_span.rows > 1) {
        out << " rowspan=\"" << cell_span.rows << "\"";
      }
      if (cell_span.columns > 1) {
        out << " colspan=\"" << cell_span.columns << "\"";
      }
      out << optional_style_attribute(translate_table_cell_style(style));
      out << ">";
      translate_children(cursor, out, config);
      out << "</td>";

      return true;
    });

    out << "</tr>";

    return true;
  });

  out << "</table>";
}

void translate_image(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig & /*config*/) {
  out << "<img style=\"position:absolute;left:0;top:0;width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (cursor.element().image().internal()) {
    auto image_file = cursor.element().image().file().value();

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
    out << cursor.element().image().href();
  }

  out << "\">";
}

void translate_frame(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config) {
  auto style = cursor.element().style(StyleContext::style_tree);

  // TODO choosing <span> by default because it is valid inside <p>;
  // alternatives?
  const bool span = cursor.element().frame().anchor_type().has_value();
  if (span) {
    out << "<span";
  } else {
    out << "<div";
  }

  out << optional_style_attribute(
      translate_frame_properties(cursor.element().frame()) +
      translate_text_style(style.text()) +
      translate_paragraph_style(style.paragraph()) +
      translate_drawing_style(style.graphic()));
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
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<div";
  out << optional_style_attribute(
      translate_rect_properties(cursor.element().rect()) +
      translate_drawing_style(style.graphic()));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void translate_line(DocumentCursor &cursor, std::ostream &out,
                    const HtmlConfig & /*config*/) {
  auto style = cursor.element().style(StyleContext::style_tree);

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optional_style_attribute("z-index:-1;position:absolute;top:0;left:0;" +
                                  translate_drawing_style(style.graphic()));
  out << ">";

  out << "<line";
  out << " x1=\"" << cursor.element().line().x1() << "\" y1=\""
      << cursor.element().line().y1() << "\"";
  out << " x2=\"" << cursor.element().line().x2() << "\" y2=\""
      << cursor.element().line().y2() << "\"";
  out << " />";

  out << "</svg>";
}

void translate_circle(DocumentCursor &cursor, std::ostream &out,
                      const HtmlConfig &config) {
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<div";
  out << optional_style_attribute(
      translate_circle_properties(cursor.element().circle()) +
      translate_drawing_style(style.graphic()));
  out << ">";
  translate_children(cursor, out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)";
  out << "</div>";
}

void translate_custom_shape(DocumentCursor &cursor, std::ostream &out,
                            const HtmlConfig &config) {
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<div";
  out << optional_style_attribute(
      translate_custom_shape_properties(cursor.element().custom_shape()) +
      translate_drawing_style(style.graphic()));
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
  if (cursor.element_type() == ElementType::text) {
    translate_text(cursor, out);
  } else if (cursor.element_type() == ElementType::line_break) {
    out << "<br>";
  } else if (cursor.element_type() == ElementType::paragraph) {
    translate_paragraph(cursor, out, config);
  } else if (cursor.element_type() == ElementType::span) {
    translate_span(cursor, out, config);
  } else if (cursor.element_type() == ElementType::link) {
    translate_link(cursor, out, config);
  } else if (cursor.element_type() == ElementType::bookmark) {
    translate_bookmark(cursor, out, config);
  } else if (cursor.element_type() == ElementType::list) {
    translate_list(cursor, out, config);
  } else if (cursor.element_type() == ElementType::table) {
    translate_table(cursor, out, config);
  } else if (cursor.element_type() == ElementType::frame) {
    translate_frame(cursor, out, config);
  } else if (cursor.element_type() == ElementType::image) {
    translate_image(cursor, out, config);
  } else if (cursor.element_type() == ElementType::rect) {
    translate_rect(cursor, out, config);
  } else if (cursor.element_type() == ElementType::line) {
    translate_line(cursor, out, config);
  } else if (cursor.element_type() == ElementType::circle) {
    translate_circle(cursor, out, config);
  } else if (cursor.element_type() == ElementType::custom_shape) {
    translate_custom_shape(cursor, out, config);
  } else if (cursor.element_type() == ElementType::group) {
    translate_children(cursor, out, config);
  } else {
    // TODO log
  }
}

void translate_master_page(DocumentCursor &cursor, std::ostream &out,
                           const HtmlConfig &config) {
  if (!cursor.move_to_master_page()) {
    return;
  }

  cursor.for_each_child([&](DocumentCursor &cursor, const std::uint32_t) {
    // TODO filter placeholders
    translate_element(cursor, out, config);
  });

  cursor.move_to_parent();
}

void translate_sheet(DocumentCursor &cursor, std::ostream &out,
                     const HtmlConfig &config) {
  // TODO table column width does not work
  // TODO table row height does not work
  auto style = cursor.element().style(StyleContext::style_tree);

  out << "<table";
  out << optional_style_attribute(translate_table_style(style.table()));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  auto dimensions = cursor.element().table().dimensions();
  std::uint32_t end_row = dimensions.rows;
  std::uint32_t end_column = dimensions.columns;
  if ((config.table_limit_rows > 0) && (end_row > config.table_limit_rows)) {
    end_row = config.table_limit_rows;
  }
  if ((config.table_limit_columns > 0) &&
      (end_column > config.table_limit_columns)) {
    end_column = config.table_limit_columns;
  }
  /*
  if (config.table_limit_by_content) {
    const auto content_bounds = element.content_bounds();
    const auto content_bounds_within = element.content_bounds(
        {config.table_limit_rows, config.table_limit_columns});
    end_row = end_row ? content_bounds_within.rows : content_bounds.rows;
    end_column =
        end_column ? content_bounds_within.columns : content_bounds.columns;
  }
   */

  std::uint32_t column_index = 0;
  std::uint32_t row_index = 0;

  cursor.for_each_column([&](DocumentCursor &cursor, const std::uint32_t) {
    auto style = cursor.element().style(StyleContext::style_tree);

    out << "<col";
    out << optional_style_attribute(
        translate_table_column_style(style.table_column()));
    out << ">";

    ++column_index;
    return column_index < end_column;
  });

  cursor.for_each_row([&](DocumentCursor &cursor, const std::uint32_t) {
    auto style = cursor.element().style(StyleContext::style_tree);

    out << "<tr";
    out << optional_style_attribute(
        translate_table_row_style(style.table_row()));
    out << ">";

    column_index = 0;

    cursor.for_each_cell([&](DocumentCursor &cursor, const std::uint32_t) {
      auto style = cursor.element().style(StyleContext::style_tree);

      // TODO check if cell hidden

      auto cell_span = cursor.element().table_cell().span();

      out << "<td";
      if (cell_span.rows > 1) {
        out << " rowspan=\"" << cell_span.rows << "\"";
      }
      if (cell_span.columns > 1) {
        out << " colspan=\"" << cell_span.columns << "\"";
      }
      out << optional_style_attribute(translate_table_cell_style(style));
      out << ">";
      translate_children(cursor, out, config);
      out << "</td>";

      column_index += cell_span.columns;
      return column_index < end_column;
    });

    out << "</tr>";

    ++row_index;
    return row_index < end_row;
  });

  out << "</table>";
}

void translate_text_document(DocumentCursor &cursor, std::ostream &out,
                             const HtmlConfig &config) {
  auto style = cursor.element().style(StyleContext::style_tree);

  if (config.text_document_margin) {
    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(style.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(style.page_layout()));
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
    auto style = cursor.element().style(StyleContext::style_tree);

    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(style.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(style.page_layout()));
    out << ">";
    translate_master_page(cursor, out, config);
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

    translate_sheet(cursor, out, config);
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
    auto style = cursor.element().style(StyleContext::style_tree);

    out << "<div";
    out << optional_style_attribute(
        translate_outer_page_style(style.page_layout()));
    out << ">";
    out << "<div";
    out << optional_style_attribute(
        translate_inner_page_style(style.page_layout()));
    out << ">";
    translate_master_page(cursor, out, config);
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
