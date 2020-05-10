#include <ContentTranslator.h>
#include <StyleTranslator.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/StringUtil.h>
#include <crypto/CryptoUtil.h>
#include <cstring>
#include <glog/logging.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <pugixml.hpp>
#include <string>
#include <svm/Svm2Svg.h>
#include <unordered_map>
#include <unordered_set>

namespace odr {
namespace odf {

namespace {
void TextTranslator(const pugi::xml_text &in, std::ostream &out,
                    Context &context) {
  std::string text = in.as_string();
  common::StringUtil::findAndReplaceAll(text, "&", "&amp;");
  common::StringUtil::findAndReplaceAll(text, "<", "&lt;");
  common::StringUtil::findAndReplaceAll(text, ">", "&gt;");

  if (!context.config->editable) {
    out << text;
  } else {
    out << R"(<span contenteditable="true" data-odr-cid=")"
        << context.currentTextTranslationIndex << "\">" << text << "</span>";
    context.textTranslation[context.currentTextTranslationIndex] = &in;
    ++context.currentTextTranslationIndex;
  }
}

void StyleClassTranslator(const std::string &name, std::ostream &out,
                          Context &context) {
  out << name;

  { // handle style dependencies
    const auto it = context.styleDependencies.find(name);
    if (it == context.styleDependencies.end()) {
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

  // TODO this is odp and odg specific
  if (std::strcmp("draw:page", in.Name()) == 0) {
    out << "odr-page ";
  }

  // TODO this is ods specific
  if (std::strcmp("table:table", in.Name()) == 0) {
    out << "odr-table ";
  }
  if (!in.attribute("table:style-name")) {
    const auto it = context.defaultCellStyles.find(context.tableCursor.col());
    if (it != context.defaultCellStyles.end()) {
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

  out << "<span style=\"white-space:pre-wrap\">";
  for (std::uint32_t i = 0; i < count; ++i) {
    out << " ";
  }
  out << "</span>";
}

void TabTranslator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "<span style=\"white-space:pre-wrap\">&emsp;</span>";
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

  out << "position:absolute;";
  if (const auto widthAttr = in.attribute("svg:width"); widthAttr)
    out << "width:" << widthAttr.as_string() << ";";
  if (const auto heightAttr = in.attribute("svg:height"); heightAttr)
    out << "height:" << heightAttr.as_string() << ";";
  if (const auto xAttr = in.attribute("svg:x"); xAttr)
    out << "left:" << xAttr.as_string() << ";";
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
      const access::Path path{href};
      if (!context.storage->isFile(path)) {
        // TODO sometimes `ObjectReplacements` does not exist
        out << path;
      } else {
        std::string image =
            access::StreamUtil::read(*context.storage->read(path));
        if ((href.find("ObjectReplacements", 0) != std::string::npos) ||
            (href.find(".svm", 0) != std::string::npos)) {
          std::istringstream svmIn(image);
          std::ostringstream svgOut;
          svm::Translator::svg(svmIn, svgOut);
          image = svgOut.str();
          out << "data:image/svg+xml;base64, ";
        } else {
          // hacky image/jpg working according to tom
          out << "data:image/jpg;base64, ";
        }
        out << crypto::Util::base64Encode(image);
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
  context.tableRange = {
      {context.config->tableOffsetRows, context.config->tableOffsetCols},
      context.config->tableLimitRows,
      context.config->tableLimitCols};

  // TODO remove file check; add simple table translator for odt/odp
  if ((context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) &&
      context.config->tableLimitByDimensions) {
    const common::TablePosition end{
        context.meta->entries[context.entry].rowCount,
        context.meta->entries[context.entry].columnCount};

    context.tableRange = {context.tableRange.from(), end};
  }

  context.tableCursor = {};
  context.defaultCellStyles.clear();

  if (context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) {
    out << "<div class=\"odr-page\">";
  }

  out << "<table";
  ElementAttributeTranslator(in, out, context);
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";

  if (context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) {
    out << "</div>";
  }

  ++context.entry;
}

void TableColumnTranslator(const pugi::xml_node &in, std::ostream &out,
                           Context &context) {
  const auto repeated =
      in.attribute("table:number-columns-repeated").as_uint(1);
  const auto defaultCellStyleAttribute =
      in.attribute("table:default-cell-style-name");
  // TODO we could use span instead
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.col() >= context.tableRange.to().col())
      break;
    if (context.tableCursor.col() >= context.tableRange.from().col()) {
      if (defaultCellStyleAttribute)
        context.defaultCellStyles[context.tableCursor.col()] =
            defaultCellStyleAttribute.as_string();
      out << "<col";
      ElementAttributeTranslator(in, out, context);
      out << ">";
    }
    context.tableCursor.addCol();
  }
}

void TableRowTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  const auto repeated = in.attribute("table:number-rows-repeated").as_uint(1);
  context.tableCursor.addRow(0); // TODO hacky
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.row() >= context.tableRange.to().row())
      break;
    if (context.tableCursor.row() >= context.tableRange.from().row()) {
      out << "<tr";
      ElementAttributeTranslator(in, out, context);
      out << ">";
      ElementChildrenTranslator(in, out, context);
      out << "</tr>";
    }
    context.tableCursor.addRow();
  }
}

void TableCellTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  const auto repeated =
      in.attribute("table:number-columns-repeated").as_uint(1);
  const auto colspan = in.attribute("table:number-columns-spanned").as_uint(1);
  const auto rowspan = in.attribute("table:number-rows-spanned").as_uint(1);
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.col() >= context.tableRange.to().col())
      break;
    if (context.tableCursor.col() >= context.tableRange.from().col()) {
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
    context.tableCursor.addCell(colspan, rowspan);
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

} // namespace odf
} // namespace odr
