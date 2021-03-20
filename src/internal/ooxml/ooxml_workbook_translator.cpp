#include <cstring>
#include <glog/logging.h>
#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/crypto/crypto_util.h>
#include <internal/ooxml/ooxml_translator_context.h>
#include <internal/ooxml/ooxml_workbook_translator.h>
#include <internal/util/stream_util.h>
#include <internal/util/string_util.h>
#include <odr/html_config.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace odr::internal::ooxml {

namespace {
void fonts_translator(pugi::xml_node in, std::ostream &out, Context &) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    out << ".font-" << i << " {";

    if (const auto name = e.child("name"); name) {
      out << "font-family: " << name.attribute("val").as_string() << ";";
    }

    if (const auto size = e.child("sz"); size) {
      out << "font-size: " << size.attribute("val").as_string() << "pt;";
    }

    if (const auto color = e.child("color"); color && color.attribute("rgb")) {
      out << "color: #"
          << std::string(color.attribute("rgb").as_string()).substr(2) << ";";
    }

    // TODO
    // <u val="single" /> underline?
    // <b val="true" /> bold?
    // <vertAlign val="superscript" />

    out << "} ";

    ++i;
  }
}

void fills_translator(pugi::xml_node in, std::ostream &out, Context &) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    out << ".fill-" << i << " {";

    if (const auto pattern_fill = e.child("patternFill"); pattern_fill) {
      if (const auto bg_color = pattern_fill.child("bgColor"); bg_color) {
        out << "background-color: #"
            << std::string(bg_color.attribute("rgb").as_string()).substr(2)
            << ";";
      }
    }

    out << "} ";

    ++i;
  }
}

void borders_translator(pugi::xml_node in, std::ostream &out, Context &) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    out << ".border-" << i << " {";
    // TODO
    out << "} ";

    ++i;
  }
}

void cell_xfs_translator(pugi::xml_node in, std::ostream &out,
                         Context &context) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    const std::string name = "cellxf-" + std::to_string(i);

    if (const auto apply_font = e.attribute("applyFont");
        apply_font && (std::strcmp(apply_font.as_string(), "true") == 0 ||
                       std::strcmp(apply_font.as_string(), "1") == 0)) {
      context.style_dependencies[name].push_back(
          std::string("font-") + e.attribute("fontId").as_string());
    }

    if (const auto fill_id = e.attribute("fillId"); fill_id) {
      context.style_dependencies[name].push_back(std::string("fill-") +
                                                 fill_id.as_string());
    }

    if (const auto apply_border = e.attribute("fillId");
        apply_border && (std::strcmp(apply_border.as_string(), "true") == 0 ||
                         std::strcmp(apply_border.as_string(), "1") == 0)) {
      context.style_dependencies[name].push_back(
          std::string("border-") + e.attribute("borderId").as_string());
    }

    out << "." << name << " {";

    if (const auto apply_alignment = e.attribute("applyAlignment");
        apply_alignment &&
        (std::strcmp(apply_alignment.as_string(), "true") == 0 ||
         std::strcmp(apply_alignment.as_string(), "1") == 0)) {
      out << "text-align: "
          << e.child("alignment").attribute("horizontal").as_string() << ";";
      // TODO vertical alignment
      // <alignment horizontal="left" vertical="bottom" textRotation="0"
      // wrapText="false" indent="0" shrinkToFit="false" />
    }

    // TODO
    // <protection locked="true" hidden="false" />

    out << "} ";

    ++i;
  }
}
} // namespace

void workbook_translator::css(const pugi::xml_node &in, Context &context) {
  std::ostream &out = *context.output;

  if (const auto fonts = in.child("fonts"); fonts) {
    fonts_translator(fonts, out, context);
  }

  if (const auto fills = in.child("fills"); fills) {
    fills_translator(fills, out, context);
  }

  if (const auto borders = in.child("borders"); borders) {
    borders_translator(borders, out, context);
  }

  if (const auto cell_xfs = in.child("cellXfs"); cell_xfs) {
    cell_xfs_translator(cell_xfs, out, context);
  }
}

namespace {
void text_translator(pugi::xml_node in, std::ostream &out, Context &context) {
  std::string text = in.value();
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

void style_attribute_translator(pugi::xml_node in, std::ostream &out,
                                Context &) {
  const std::string prefix = in.name();

  const auto width = in.attribute("width");
  const auto ht = in.attribute("ht");
  if (width || ht) {
    out << " style=\"";
    if (width) {
      out << "width:" << width.as_float() << "in;";
    }
    if (ht) {
      out << "height:" << ht.as_float() << "pt;";
    }
    out << "\"";
  }
}

void element_attribute_translator(pugi::xml_node in, std::ostream &out,
                                  Context &context) {
  if (const auto s = in.attribute("s"); s) {
    const std::string name = std::string("cellxf-") + s.as_string();
    out << " class=\"";
    out << name;

    { // handle style dependencies
      const auto it = context.style_dependencies.find(name);
      if (it == std::end(context.style_dependencies)) {
        // TODO remove ?
        LOG(WARNING) << "unknown style: " << name;
      } else {
        for (auto i = it->second.rbegin(); i != it->second.rend(); ++i) {
          out << " " << *i;
        }
      }
    }

    out << "\"";
  }

  style_attribute_translator(in, out, context);
}

void element_children_translator(pugi::xml_node in, std::ostream &out,
                                 Context &context);
void element_translator(pugi::xml_node in, std::ostream &out, Context &context);

void table_translator(pugi::xml_node in, std::ostream &out, Context &context) {
  // TODO context.config->tableLimitByDimensions
  context.table_range = {{0, 0},
                         context.config->table_limit_rows,
                         context.config->table_limit_cols};
  context.table_cursor = {};

  out << R"(<table border="0" cellspacing="0" cellpadding="0")";
  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << "</table>";
}

void table_col_translator(pugi::xml_node in, std::ostream &out,
                          Context &context) {
  // TODO if min/max is unordered we have a problem here; fail fast in that case

  const auto min = in.attribute("min").as_uint(1);
  const auto max = in.attribute("max").as_uint(1);
  const auto repeated = max - min + 1;

  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.table_cursor.col() >= context.table_range.to().col()) {
      break;
    }
    if (context.table_cursor.col() >= context.table_range.from().col()) {
      out << "<col";
      element_attribute_translator(in, out, context);
      out << ">";
    }
    context.table_cursor.add_col();
  }
}

void table_row_translator(pugi::xml_node in, std::ostream &out,
                          Context &context) {
  const auto row_index = in.attribute("r").as_uint() - 1;

  while (row_index > context.table_cursor.row()) {
    if (context.table_cursor.row() >= context.table_range.to().row()) {
      return;
    }
    if (context.table_cursor.row() >= context.table_range.from().row()) {
      // TODO insert empty proper rows
      out << "<tr></tr>";
    }
    context.table_cursor.add_row();
  }

  context.table_cursor.add_row(0); // TODO hacky
  if (context.table_cursor.row() >= context.table_range.to().row()) {
    return;
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

void table_cell_translator(pugi::xml_node in, std::ostream &out,
                           Context &context) {
  const common::TablePosition cell_index(in.attribute("r").as_string());

  while (cell_index.col() > context.table_cursor.col()) {
    if (context.table_cursor.col() >= context.table_range.to().col()) {
      return;
    }
    if (context.table_cursor.col() >= context.table_range.from().col()) {
      out << "<td></td>";
    }
    context.table_cursor.add_cell();
  }

  out << "<td";
  element_attribute_translator(in, out, context);
  out << ">";

  if (const auto t = in.attribute("t"); t) {
    if (std::strcmp(t.as_string(), "s") == 0) {
      const auto shared_string_index = in.child("v").text().as_int(-1);
      if (shared_string_index >= 0) {
        pugi::xml_node replacement =
            context.shared_strings[shared_string_index];
        element_children_translator(replacement, out, context);
      } else {
        DLOG(INFO) << "undefined behaviour: shared string not found";
      }
    } else if ((std::strcmp(t.as_string(), "str") == 0) ||
               (std::strcmp(t.as_string(), "inlineStr") == 0) ||
               (std::strcmp(t.as_string(), "n") == 0)) {
      element_children_translator(in, out, context);
    } else {
      DLOG(INFO) << "undefined behaviour: t=" << t.as_string();
    }
  } else {
    // TODO empty cell?
  }

  out << "</td>";
  context.table_cursor.add_cell();
}

void element_children_translator(pugi::xml_node in, std::ostream &out,
                                 Context &context) {
  for (auto &&n : in) {
    if (n.type() == pugi::node_pcdata) {
      text_translator(n, out, context);
    } else if (n.type() == pugi::node_element) {
      element_translator(n, out, context);
    }
  }
}

void element_translator(pugi::xml_node in, std::ostream &out,
                        Context &context) {
  static std::unordered_map<std::string, const char *> substitution{
      {"cols", "colgroup"},
  };
  static std::unordered_set<std::string> skippers{
      "headerFooter",
      "f", // TODO translate formula and hide
  };

  const std::string element = in.name();
  if (skippers.find(element) != std::end(skippers)) {
    return;
  }

  if (element == "worksheet") {
    table_translator(in, out, context);
  } else if (element == "col") {
    table_col_translator(in, out, context);
  } else if (element == "row") {
    table_row_translator(in, out, context);
  } else if (element == "c") {
    table_cell_translator(in, out, context);
  } else {
    const auto it = substitution.find(element);
    if (it != std::end(substitution)) {
      out << "<" << it->second;
      element_attribute_translator(in, out, context);
      out << ">";
    }
    element_children_translator(in, out, context);
    if (it != std::end(substitution)) {
      out << "</" << it->second << ">";
    }
  }
}
} // namespace

void workbook_translator::html(const pugi::xml_node &in, Context &context) {
  element_translator(in, *context.output, context);
}

} // namespace odr::internal::ooxml
