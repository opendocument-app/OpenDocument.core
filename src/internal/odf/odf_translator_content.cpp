#include <cstring>
#include <glog/logging.h>
#include <internal/abstract/filesystem.h>
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
void TextTranslator(const pugi::xml_text &in, std::ostream &out,
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

void StyleClassTranslator(const std::string &name, std::ostream &out,
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

void StyleClassTranslator(const pugi::xml_node &in, std::ostream &out,
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
      StyleClassTranslator(it->second, out, context);
      out << " ";
    }
  }
  if (const auto valueTypeAttr = in.attribute("office:value-type");
      valueTypeAttr)
    out << "odr-value-type-" << valueTypeAttr.as_string() << " ";

  for (auto &&a : in.attributes()) {
    const std::string attribute = a.name();
    if (styleAttributes.find(attribute) == styleAttributes.end())
      continue;
    std::string name = StyleTranslator::escapeStyleName(a.as_string());
    if (attribute == "draw:master-page-name")
      name = StyleTranslator::escapeMasterStyleName(a.as_string());
    StyleClassTranslator(name, out, context);
    out << " ";
  }
  out << "\"";
}

void ElementAttributeTranslator(const pugi::xml_node &in, std::ostream &out,
                                Context &context) {
  StyleClassTranslator(in, out, context);
}

void ElementChildrenTranslator(const pugi::xml_node &in, std::ostream &out,
                               Context &context);
void ElementTranslator(const pugi::xml_node &in, std::ostream &out,
                       Context &context);

void ParagraphTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  out << "<p";
  ElementAttributeTranslator(in, out, context);
  out << ">";

  if (in.first_child())
    ElementChildrenTranslator(in, out, context);
  else
    out << "<br>";

  out << "</p>";
}

void SpaceTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto count = in.attribute("text:c").as_uint(1);
  if (count <= 0)
    return;

  out << "<span class=\"odr-whitespace\">";
  for (std::uint32_t i = 0; i < count; ++i) {
    out << " ";
  }
  out << "</span>";
}

void TabTranslator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "<span class=\"odr-whitespace\">&emsp;</span>";
}

void LineBreakTranslator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "<br>";
}

void LinkTranslator(const pugi::xml_node &in, std::ostream &out,
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
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</a>";
}

void BookmarkTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  out << "<a";
  if (const auto id = in.attribute("text:name"); id) {
    out << " id=\"" << id.as_string() << "\"";
  } else {
    LOG(WARNING) << "empty bookmark";
  }
  ElementAttributeTranslator(in, out, context);
  out << ">";

  out << "</a>";
}

void FrameTranslator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << "<div style=\"";

  if (const auto widthAttr = in.attribute("svg:width"); widthAttr)
    out << "width:" << widthAttr.as_string() << ";";
  if (const auto heightAttr = in.attribute("svg:height"); heightAttr)
    out << "height:" << heightAttr.as_string() << ";";
  if (const auto xAttr = in.attribute("svg:x"); xAttr)
    out << "position:absolute;left:" << xAttr.as_string() << ";";
  if (const auto yAttr = in.attribute("svg:y"); yAttr)
    out << "top:" << yAttr.as_string() << ";";

  out << "\"";

  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</div>";
}

void ImageTranslator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << "<img style=\"width:100%;height:100%\"";

  if (const auto hrefAttr = in.attribute("xlink:href"); hrefAttr) {
    const std::string href = hrefAttr.as_string();
    out << " alt=\"Error: image not found or unsupported: " << href << "\"";
    out << " src=\"";
    try {
      const common::Path path{href};
      if (!context.filesystem->is_file(path)) {
        // TODO sometimes `ObjectReplacements` does not exist
        out << path;
      } else {
        std::string image;
        if ((href.find("ObjectReplacements", 0) != std::string::npos) ||
            (href.find(".svm", 0) != std::string::npos)) {
          svm::SvmFile svm_file(context.filesystem->open(path));
          std::ostringstream svg_out;
          svm::Translator::svg(svm_file, svg_out);
          image = svg_out.str();
          out << "data:image/svg+xml;base64, ";
        } else {
          image = util::stream::read(*context.filesystem->open(path)->read());
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

  ElementAttributeTranslator(in, out, context);
  out << ">";
  // TODO children for image?
  ElementChildrenTranslator(in, out, context);
  out << "</img>";
}

void TableTranslator(const pugi::xml_node &in, std::ostream &out,
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
  ElementAttributeTranslator(in, out, context);
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";

  ++context.entry;
}

void TableColumnTranslator(const pugi::xml_node &in, std::ostream &out,
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
      if (default_cell_style_attribute)
        context.default_cell_styles[context.table_cursor.col()] =
            default_cell_style_attribute.as_string();
      out << "<col";
      ElementAttributeTranslator(in, out, context);
      out << ">";
    }
    context.table_cursor.add_col();
  }
}

void TableRowTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  const auto repeated = in.attribute("table:number-rows-repeated").as_uint(1);
  context.table_cursor.add_row(0); // TODO hacky
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.table_cursor.row() >= context.table_range.to().row())
      break;
    if (context.table_cursor.row() >= context.table_range.from().row()) {
      out << "<tr";
      ElementAttributeTranslator(in, out, context);
      out << ">";
      ElementChildrenTranslator(in, out, context);
      out << "</tr>";
    }
    context.table_cursor.add_row();
  }
}

void TableCellTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  const auto repeated =
      in.attribute("table:number-columns-repeated").as_uint(1);
  const auto colspan = in.attribute("table:number-columns-spanned").as_uint(1);
  const auto rowspan = in.attribute("table:number-rows-spanned").as_uint(1);
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.table_cursor.col() >= context.table_range.to().col())
      break;
    if (context.table_cursor.col() >= context.table_range.from().col()) {
      out << "<td";
      ElementAttributeTranslator(in, out, context);
      // TODO check for >1?
      if (in.attribute("table:number-columns-spanned"))
        out << " colspan=\"" << colspan << "\"";
      if (in.attribute("table:number-rows-spanned"))
        out << " rowspan=\"" << rowspan << "\"";
      out << ">";
      ElementChildrenTranslator(in, out, context);
      out << "</td>";
    }
    context.table_cursor.add_cell(colspan, rowspan);
  }
}

void DrawLineTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  const auto x1 = in.attribute("svg:x1");
  const auto y1 = in.attribute("svg:y1");
  const auto x2 = in.attribute("svg:x2");
  const auto y2 = in.attribute("svg:y2");

  if (!x1 || !y1 || !x2 || !y2)
    return;

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" style="z-index:-1;position:absolute;top:0;left:0;")";

  ElementAttributeTranslator(in, out, context);
  out << ">";

  out << "<line";

  out << " x1=\"" << x1.as_string() << "\"";
  out << " y1=\"" << y1.as_string() << "\"";
  out << " x2=\"" << x2.as_string() << "\"";
  out << " y2=\"" << y2.as_string() << "\"";
  out << " />";

  out << "</svg>";
}

void DrawRectTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  out << "<div style=\"";

  out << "position:absolute;";
  if (const auto width = in.attribute("svg:width"); width)
    out << "width:" << width.as_string() << ";";
  if (const auto height = in.attribute("svg:height"); height)
    out << "height:" << height.as_string() << ";";
  if (const auto x = in.attribute("svg:x"); x)
    out << "left:" << x.as_string() << ";";
  if (const auto y = in.attribute("svg:y"); y)
    out << "top:" << y.as_string() << ";";
  out << "\"";

  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%"></rect></svg>)";
  out << "</div>";
}

void DrawCircleTranslator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  out << "<div style=\"";

  out << "position:absolute;";
  if (const auto width = in.attribute("svg:width"); width)
    out << "width:" << width.as_string() << ";";
  if (const auto height = in.attribute("svg:height"); height)
    out << "height:" << height.as_string() << ";";
  if (const auto x = in.attribute("svg:x"); x)
    out << "left:" << x.as_string() << ";";
  if (const auto y = in.attribute("svg:y"); y)
    out << "top:" << y.as_string() << ";";
  out << "\"";

  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%"></rect></svg>)";
  out << "</div>";
}

void ElementChildrenTranslator(const pugi::xml_node &in, std::ostream &out,
                               Context &context) {
  for (auto &&n : in) {
    if (n.type() == pugi::node_pcdata)
      TextTranslator(n.text(), out, context);
    else if (n.type() == pugi::node_element)
      ElementTranslator(n, out, context);
  }
}

void ElementTranslator(const pugi::xml_node &in, std::ostream &out,
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
  if (skippers.find(element) != skippers.end())
    return;

  if (element == "text:p" || element == "text:h")
    ParagraphTranslator(in, out, context);
  else if (element == "text:s")
    SpaceTranslator(in, out, context);
  else if (element == "text:tab")
    TabTranslator(in, out, context);
  else if (element == "text:line-break")
    LineBreakTranslator(in, out, context);
  else if (element == "text:a")
    LinkTranslator(in, out, context);
  else if (element == "text:bookmark" || element == "text:bookmark-start")
    BookmarkTranslator(in, out, context);
  else if (element == "draw:frame" || element == "draw:custom-shape")
    FrameTranslator(in, out, context);
  else if (element == "draw:image")
    ImageTranslator(in, out, context);
  else if (element == "table:table")
    TableTranslator(in, out, context);
  else if (element == "table:table-column")
    TableColumnTranslator(in, out, context);
  else if (element == "table:table-row")
    TableRowTranslator(in, out, context);
  else if (element == "table:table-cell")
    TableCellTranslator(in, out, context);
  else if (element == "draw:line")
    DrawLineTranslator(in, out, context);
  else if (element == "draw:rect")
    DrawRectTranslator(in, out, context);
  else if (element == "draw:circle")
    DrawCircleTranslator(in, out, context);
  else {
    const auto it = substitution.find(element);
    if (it != substitution.end()) {
      out << "<" << it->second;
      ElementAttributeTranslator(in, out, context);
      out << ">";
    }
    ElementChildrenTranslator(in, out, context);
    if (it != substitution.end())
      out << "</" << it->second << ">";
  }
}
} // namespace

void ContentTranslator::html(const pugi::xml_node &in, Context &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace odr::internal::odf
