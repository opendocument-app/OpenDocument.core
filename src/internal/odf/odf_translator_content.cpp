#include <cstring>
#include <glog/logging.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/crypto/crypto_util.h>
#include <internal/odf/odf_translator_content.h>
#include <internal/odf/odf_translator_context.h>
#include <internal/odf/odf_translator_style.h>
#include <internal/svm/svm_file.h>
#include <internal/svm/svm_to_svg.h>
#include <internal/util/stream_util.h>
#include <internal/util/string_util.h>
#include <odr/file_meta.h>
#include <odr/html_config.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace odr::internal::odf {

namespace {
void text_translator(const pugi::xml_text &in, std::ostream &out,
                     Context &context) {
  std::string text = in.as_string();
  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");

  if (!context.config->editable) {
    out << text;
  } else {
    out << R"(<span contenteditable="true" data-odr-cid=")"
        << context.current_text_translation_index << "\">" << text << "</span>";
    context.text_translation[context.current_text_translation_index] = in;
    ++context.current_text_translation_index;
  }
}

void style_class_translator(const std::string &name, std::ostream &out,
                            Context &context) {
  out << name;

  { // handle style dependencies
    const auto it = context.style_dependencies.find(name);
    if (it == context.style_dependencies.end()) {
      // TODO remove ?
      LOG(WARNING) << "unknown style: " << name;
    } else {
      for (auto i = it->second.rbegin(); i != it->second.rend(); ++i) {
        out << " " << *i;
      }
    }
  }
}

void style_class_translator(const pugi::xml_node &in, std::ostream &out,
                            Context &context) {
  static std::unordered_set<std::string> styleAttributes{
      "text:style-name",         "table:style-name",
      "draw:style-name",         "draw:text-style-name",
      "presentation:style-name", "draw:master-page-name",
  };

  out << " class=\"";

  // TODO this is ods specific
  if (!in.attribute("table:style-name")) {
    const auto it =
        context.default_cell_styles.find(context.table_cursor.col());
    if (it != context.default_cell_styles.end()) {
      style_class_translator(it->second, out, context);
      out << " ";
    }
  }
  if (const auto value_type_attr = in.attribute("office:value-type");
      value_type_attr) {
    out << "odr-value-type-" << value_type_attr.as_string() << " ";
  }

  for (auto &&a : in.attributes()) {
    const std::string attribute = a.name();
    if (styleAttributes.find(attribute) == styleAttributes.end()) {
      continue;
    }
    std::string name = style_translator::escape_style_name(a.as_string());
    if (attribute == "draw:master-page-name") {
      name = style_translator::escape_master_style_name(a.as_string());
    }
    style_class_translator(name, out, context);
    out << " ";
  }
  out << "\"";
}

void element_attribute_translator(const pugi::xml_node &in, std::ostream &out,
                                  Context &context) {
  style_class_translator(in, out, context);
}

void element_children_translator(const pugi::xml_node &in, std::ostream &out,
                                 Context &context);
void element_translator(const pugi::xml_node &in, std::ostream &out,
                        Context &context);

void paragraph_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  out << "<p";
  element_attribute_translator(in, out, context);
  out << ">";

  if (in.first_child()) {
    element_children_translator(in, out, context);
  } else {
    out << "<br>";
  }

  out << "</p>";
}

void space_translator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto count = in.attribute("text:c").as_uint(1);
  if (count <= 0) {
    return;
  }

  out << "<span class=\"odr-whitespace\">";
  for (std::uint32_t i = 0; i < count; ++i) {
    out << " ";
  }
  out << "</span>";
}

void tab_translator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "<span class=\"odr-whitespace\">&emsp;</span>";
}

void line_break_translator(const pugi::xml_node &, std::ostream &out,
                           Context &) {
  out << "<br>";
}

void link_translator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << "<a";
  if (const auto href = in.attribute("xlink:href"); href) {
    out << " href=\"" << href.as_string() << "\"";
    // NOTE: there is a trim in java
    if ((std::strlen(href.as_string()) > 0) && (href.as_string()[0] == '#')) {
      out << " target=\"_self\"";
    }
  } else {
    LOG(WARNING) << "empty link";
  }
  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << "</a>";
}

void bookmark_translator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  out << "<a";
  if (const auto id = in.attribute("text:name"); id) {
    out << " id=\"" << id.as_string() << "\"";
  } else {
    LOG(WARNING) << "empty bookmark";
  }
  element_attribute_translator(in, out, context);
  out << ">";

  out << "</a>";
}

void frame_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  out << "<div style=\"";

  if (const auto width_attr = in.attribute("svg:width"); width_attr) {
    out << "width:" << width_attr.as_string() << ";";
  }
  if (const auto height_attr = in.attribute("svg:height"); height_attr) {
    out << "height:" << height_attr.as_string() << ";";
  }
  if (const auto x_attr = in.attribute("svg:x"); x_attr) {
    out << "position:absolute;left:" << x_attr.as_string() << ";";
  }
  if (const auto y_attr = in.attribute("svg:y"); y_attr) {
    out << "top:" << y_attr.as_string() << ";";
  }

  out << "\"";

  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << "</div>";
}

void image_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  out << "<img style=\"width:100%;height:100%\"";

  if (const auto href_attr = in.attribute("xlink:href"); href_attr) {
    const std::string href = href_attr.as_string();
    out << " alt=\"Error: image not found or unsupported: " << href << "\"";
    out << " src=\"";
    try {
      const common::Path path{href};
      if (!context.filesystem->is_file(path)) {
        // TODO sometimes `ObjectReplacements` does not exist
        out << path;
      } else {
        std::string image =
            util::stream::read(*context.filesystem->open(path)->read());
        if ((href.find("ObjectReplacements", 0) != std::string::npos) ||
            (href.find(".svm", 0) != std::string::npos)) {
          // TODO tellg does not work on the istream of a zip file
          svm::SvmFile svm_file(std::make_shared<common::MemoryFile>(image));
          std::ostringstream svg_out;
          svm::Translator::svg(svm_file, svg_out);
          image = svg_out.str();
          out << "data:image/svg+xml;base64, ";
        } else {
          // hacky image/jpg working according to tom
          out << "data:image/jpg;base64, ";
        }
        out << crypto::util::base64_encode(image);
      }
    } catch (...) {
      out << href;
    }
    out << "\"";
  } else {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  }

  element_attribute_translator(in, out, context);
  out << ">";
  // TODO children for image?
  element_children_translator(in, out, context);
  out << "</img>";
}

void table_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  context.table_range = {{0, 0},
                         context.config->table_limit_rows,
                         context.config->table_limit_cols};

  // TODO remove file check; add simple table translator for odt/odp
  if ((context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) &&
      context.config->table_limit_by_dimensions) {
    const common::TablePosition end{
        std::min(context.config->table_limit_rows,
                 context.meta->entries[context.entry].row_count),
        std::min(context.config->table_limit_cols,
                 context.meta->entries[context.entry].column_count)};

    context.table_range = {context.table_range.from(), end};
  }

  context.table_cursor = {};
  context.default_cell_styles.clear();

  out << "<table";
  element_attribute_translator(in, out, context);
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";
  element_children_translator(in, out, context);
  out << "</table>";

  ++context.entry;
}

void table_column_translator(const pugi::xml_node &in, std::ostream &out,
                             Context &context) {
  const auto repeated =
      in.attribute("table:number-columns-repeated").as_uint(1);
  const auto default_cell_style_attribute =
      in.attribute("table:default-cell-style-name");
  // TODO we could use span instead
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.table_cursor.col() >= context.table_range.to().col())
      break;
    if (context.table_cursor.col() >= context.table_range.from().col()) {
      if (default_cell_style_attribute) {
        context.default_cell_styles[context.table_cursor.col()] =
            default_cell_style_attribute.as_string();
      }
      out << "<col";
      element_attribute_translator(in, out, context);
      out << ">";
    }
    context.table_cursor.add_col();
  }
}

void table_row_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  const auto repeated = in.attribute("table:number-rows-repeated").as_uint(1);
  context.table_cursor.add_row(0); // TODO hacky
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.table_cursor.row() >= context.table_range.to().row()) {
      break;
    }
    if (context.table_cursor.row() >= context.table_range.from().row()) {
      out << "<tr";
      element_attribute_translator(in, out, context);
      out << ">";
      element_children_translator(in, out, context);
      out << "</tr>";
    }
    context.table_cursor.add_row();
  }
}

void table_cell_translator(const pugi::xml_node &in, std::ostream &out,
                           Context &context) {
  const auto repeated =
      in.attribute("table:number-columns-repeated").as_uint(1);
  const auto colspan = in.attribute("table:number-columns-spanned").as_uint(1);
  const auto rowspan = in.attribute("table:number-rows-spanned").as_uint(1);
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.table_cursor.col() >= context.table_range.to().col()) {
      break;
    }
    if (context.table_cursor.col() >= context.table_range.from().col()) {
      out << "<td";
      element_attribute_translator(in, out, context);
      // TODO check for >1?
      if (in.attribute("table:number-columns-spanned")) {
        out << " colspan=\"" << colspan << "\"";
      }
      if (in.attribute("table:number-rows-spanned")) {
        out << " rowspan=\"" << rowspan << "\"";
      }
      out << ">";
      element_children_translator(in, out, context);
      out << "</td>";
    }
    context.table_cursor.add_cell(colspan, rowspan);
  }
}

void draw_line_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  const auto x1 = in.attribute("svg:x1");
  const auto y1 = in.attribute("svg:y1");
  const auto x2 = in.attribute("svg:x2");
  const auto y2 = in.attribute("svg:y2");

  if (!x1 || !y1 || !x2 || !y2) {
    return;
  }

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" style="z-index:-1;position:absolute;top:0;left:0;")";

  element_attribute_translator(in, out, context);
  out << ">";

  out << "<line";

  out << " x1=\"" << x1.as_string() << "\"";
  out << " y1=\"" << y1.as_string() << "\"";
  out << " x2=\"" << x2.as_string() << "\"";
  out << " y2=\"" << y2.as_string() << "\"";
  out << " />";

  out << "</svg>";
}

void draw_rect_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  out << "<div style=\"";

  out << "position:absolute;";
  if (const auto width = in.attribute("svg:width"); width) {
    out << "width:" << width.as_string() << ";";
  }
  if (const auto height = in.attribute("svg:height"); height) {
    out << "height:" << height.as_string() << ";";
  }
  if (const auto x = in.attribute("svg:x"); x) {
    out << "left:" << x.as_string() << ";";
  }
  if (const auto y = in.attribute("svg:y"); y) {
    out << "top:" << y.as_string() << ";";
  }
  out << "\"";

  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%"></rect></svg>)";
  out << "</div>";
}

void draw_circle_translator(const pugi::xml_node &in, std::ostream &out,
                            Context &context) {
  out << "<div style=\"";

  out << "position:absolute;";
  if (const auto width = in.attribute("svg:width"); width) {
    out << "width:" << width.as_string() << ";";
  }
  if (const auto height = in.attribute("svg:height"); height) {
    out << "height:" << height.as_string() << ";";
  }
  if (const auto x = in.attribute("svg:x"); x) {
    out << "left:" << x.as_string() << ";";
  }
  if (const auto y = in.attribute("svg:y"); y) {
    out << "top:" << y.as_string() << ";";
  }
  out << "\"";

  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%"></rect></svg>)";
  out << "</div>";
}

void element_children_translator(const pugi::xml_node &in, std::ostream &out,
                                 Context &context) {
  for (auto &&n : in) {
    if (n.type() == pugi::node_pcdata) {
      text_translator(n.text(), out, context);
    } else if (n.type() == pugi::node_element) {
      element_translator(n, out, context);
    }
  }
}

void element_translator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  static std::unordered_map<std::string, const char *> substitution{
      {"text:span", "span"},
      {"text:list", "ul"},
      {"text:list-item", "li"},
      {"draw:page", "div"},
  };
  static std::unordered_set<std::string> skippers{
      "svg:desc",
      // odt
      "text:index-title-template",
      // odp
      "presentation:notes",
      // ods
      "office:annotation",
      "table:tracked-changes",
      "table:covered-table-cell",
  };

  const std::string element = in.name();
  if (skippers.find(element) != skippers.end()) {
    return;
  }

  if (element == "text:p" || element == "text:h") {
    paragraph_translator(in, out, context);
  } else if (element == "text:s") {
    space_translator(in, out, context);
  } else if (element == "text:tab") {
    tab_translator(in, out, context);
  } else if (element == "text:line-break") {
    line_break_translator(in, out, context);
  } else if (element == "text:a") {
    link_translator(in, out, context);
  } else if (element == "text:bookmark" || element == "text:bookmark-start") {
    bookmark_translator(in, out, context);
  } else if (element == "draw:frame" || element == "draw:custom-shape") {
    frame_translator(in, out, context);
  } else if (element == "draw:image") {
    image_translator(in, out, context);
  } else if (element == "table:table") {
    table_translator(in, out, context);
  } else if (element == "table:table-column") {
    table_column_translator(in, out, context);
  } else if (element == "table:table-row") {
    table_row_translator(in, out, context);
  } else if (element == "table:table-cell") {
    table_cell_translator(in, out, context);
  } else if (element == "draw:line") {
    draw_line_translator(in, out, context);
  } else if (element == "draw:rect") {
    draw_rect_translator(in, out, context);
  } else if (element == "draw:circle") {
    draw_circle_translator(in, out, context);
  } else {
    const auto it = substitution.find(element);
    if (it != substitution.end()) {
      out << "<" << it->second;
      element_attribute_translator(in, out, context);
      out << ">";
    }
    element_children_translator(in, out, context);
    if (it != substitution.end()) {
      out << "</" << it->second << ">";
    }
  }
}
} // namespace

void content_translator::html(const pugi::xml_node &in, Context &context) {
  element_translator(in, *context.output, context);
}

} // namespace odr::internal::odf
