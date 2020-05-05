#include <ContentTranslator.h>
#include <StyleTranslator.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/StringUtil.h>
#include <common/XmlUtil.h>
#include <crypto/CryptoUtil.h>
#include <glog/logging.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <string>
#include <svm/Svm2Svg.h>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>

namespace odr {
namespace odf {

namespace {
void TextTranslator(const tinyxml2::XMLText &in, std::ostream &out,
                    Context &context) {
  std::string text = in.Value();
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

void StyleClassTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                          Context &context) {
  static std::unordered_set<std::string> styleAttributes{
      "text:style-name",         "table:style-name",
      "draw:style-name",         "draw:text-style-name",
      "presentation:style-name", "draw:master-page-name",
  };

  out << " class=\"";

  // TODO this is ods specific
  if (in.FindAttribute("table:style-name") == nullptr) {
    const auto it = context.defaultCellStyles.find(context.tableCursor.col());
    if (it != context.defaultCellStyles.end()) {
      StyleClassTranslator(it->second, out, context);
      out << " ";
    }
  }
  const auto valueTypeAttr = in.FindAttribute("office:value-type");
  if (valueTypeAttr != nullptr) {
    out << "odr-value-type-" << valueTypeAttr->Value() << " ";
  }

  common::XmlUtil::visitElementAttributes(
      in, [&](const tinyxml2::XMLAttribute &a) {
        const std::string attribute = a.Name();
        if (styleAttributes.find(attribute) == styleAttributes.end())
          return;
        std::string name = StyleTranslator::escapeStyleName(a.Value());
        if (attribute == "draw:master-page-name")
          name = StyleTranslator::escapeMasterStyleName(a.Value());
        StyleClassTranslator(name, out, context);
        out << " ";
      });
  out << "\"";
}

void ElementAttributeTranslator(const tinyxml2::XMLElement &in,
                                std::ostream &out, Context &context) {
  StyleClassTranslator(in, out, context);
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out, Context &context);
void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       Context &context);

void ParagraphTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &context) {
  out << "<p";
  ElementAttributeTranslator(in, out, context);
  out << ">";

  if (in.FirstChild() == nullptr)
    out << "<br>";
  else
    ElementChildrenTranslator(in, out, context);

  out << "</p>";
}

void SpaceTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &) {
  const auto count = in.Unsigned64Attribute("text:c", 1);
  if (count <= 0) {
    return;
  }
  out << "<span class=\"odr-whitespace\">";
  for (std::uint32_t i = 0; i < count; ++i) {
    out << " ";
  }
  out << "</span>";
}

void TabTranslator(const tinyxml2::XMLElement &, std::ostream &out, Context &) {
  out << "<span class=\"odr-whitespace\">&emsp;</span>";
}

void LineBreakTranslator(const tinyxml2::XMLElement &, std::ostream &out,
                         Context &) {
  out << "<br>";
}

void LinkTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                    Context &context) {
  out << "<a";
  const auto href = in.FindAttribute("xlink:href");
  if (href != nullptr) {
    out << " href=\"" << href->Value() << "\"";
    // NOTE: there is a trim in java
    if ((std::strlen(href->Value()) > 0) && (href->Value()[0] == '#')) {
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

void BookmarkTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &context) {
  out << "<a";
  const auto id = in.FindAttribute("text:name");
  if (id != nullptr) {
    out << " id=\"" << id->Value() << "\"";
  } else {
    LOG(WARNING) << "empty bookmark";
  }
  ElementAttributeTranslator(in, out, context);
  out << ">";

  out << "</a>";
}

void FrameTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &context) {
  out << "<div style=\"";

  const auto width = in.FindAttribute("svg:width");
  const auto height = in.FindAttribute("svg:height");
  const auto x = in.FindAttribute("svg:x");
  const auto y = in.FindAttribute("svg:y");

  if (width != nullptr)
    out << "width:" << width->Value() << ";";
  if (height != nullptr)
    out << "height:" << height->Value() << ";";
  if (x != nullptr)
    out << "left:" << x->Value() << ";";
  if (y != nullptr)
    out << "top:" << y->Value() << ";";

  out << "\"";

  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</div>";
}

void ImageTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &context) {
  out << "<img style=\"width:100%;height:100%\"";

  const auto hrefAttr = in.FindAttribute("xlink:href");
  if (hrefAttr == nullptr) {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  } else {
    const std::string href = hrefAttr->Value();
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
  }

  ElementAttributeTranslator(in, out, context);
  out << ">";
  // TODO children for image?
  ElementChildrenTranslator(in, out, context);
  out << "</img>";
}

void TableTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &context) {
  context.tableRange = {
      {context.config->tableOffsetRows, context.config->tableOffsetCols},
      context.config->tableLimitRows,
      context.config->tableLimitCols};
  context.tableCursor = {};

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

  out << "<table";
  ElementAttributeTranslator(in, out, context);
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";

  ++context.entry;
}

void TableColumnTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                           Context &context) {
  const auto repeated =
      in.Unsigned64Attribute("table:number-columns-repeated", 1);
  const auto defaultCellStyleAttribute =
      in.FindAttribute("table:default-cell-style-name");
  // TODO we could use span instead
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.col() >= context.tableRange.to().col())
      break;
    if (context.tableCursor.col() >= context.tableRange.from().col()) {
      if (defaultCellStyleAttribute != nullptr) {
        context.defaultCellStyles[context.tableCursor.col()] =
            defaultCellStyleAttribute->Value();
      }
      out << "<col";
      ElementAttributeTranslator(in, out, context);
      out << ">";
    }
    context.tableCursor.addCol();
  }
}

void TableRowTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &context) {
  const auto repeated = in.Unsigned64Attribute("table:number-rows-repeated", 1);
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

void TableCellTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &context) {
  const auto repeated =
      in.Unsigned64Attribute("table:number-columns-repeated", 1);
  const auto colspan =
      in.Unsigned64Attribute("table:number-columns-spanned", 1);
  const auto rowspan = in.Unsigned64Attribute("table:number-rows-spanned", 1);
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.col() >= context.tableRange.to().col())
      break;
    if (context.tableCursor.col() >= context.tableRange.from().col()) {
      out << "<td";
      ElementAttributeTranslator(in, out, context);
      // TODO check for >1?
      if (in.FindAttribute("table:number-columns-spanned") != nullptr)
        out << " colspan=\"" << colspan << "\"";
      if (in.FindAttribute("table:number-rows-spanned") != nullptr)
        out << " rowspan=\"" << rowspan << "\"";
      out << ">";
      ElementChildrenTranslator(in, out, context);
      out << "</td>";
    }
    context.tableCursor.addCell(colspan, rowspan);
  }
}

void DrawLineTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &context) {
  const auto x1 = in.FindAttribute("svg:x1");
  const auto y1 = in.FindAttribute("svg:y1");
  const auto x2 = in.FindAttribute("svg:x2");
  const auto y2 = in.FindAttribute("svg:y2");

  if ((x1 == nullptr) || (y1 == nullptr) || (x2 == nullptr) || (y2 == nullptr))
    return;

  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" style="z-index:-1;position:absolute;top:0;left:0;")";

  ElementAttributeTranslator(in, out, context);
  out << ">";

  out << "<line";

  out << " x1=\"" << x1->Value() << "\"";
  out << " y1=\"" << y1->Value() << "\"";
  out << " x2=\"" << x2->Value() << "\"";
  out << " y2=\"" << y2->Value() << "\"";
  out << " />";

  out << "</svg>";
}

void DrawRectTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &context) {
  out << "<div style=\"";

  const auto width = in.FindAttribute("svg:width");
  const auto height = in.FindAttribute("svg:height");
  const auto x = in.FindAttribute("svg:x");
  const auto y = in.FindAttribute("svg:y");

  out << "position:absolute;";
  if (width != nullptr)
    out << "width:" << width->Value() << ";";
  if (height != nullptr)
    out << "height:" << height->Value() << ";";
  if (x != nullptr)
    out << "left:" << x->Value() << ";";
  if (y != nullptr)
    out << "top:" << y->Value() << ";";
  out << "\"";

  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%"></rect></svg>)";
  out << "</div>";
}

void DrawCircleTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                          Context &context) {
  out << "<div style=\"";

  const auto width = in.FindAttribute("svg:width");
  const auto height = in.FindAttribute("svg:height");
  const auto x = in.FindAttribute("svg:x");
  const auto y = in.FindAttribute("svg:y");

  out << "position:absolute;";
  if (width != nullptr)
    out << "width:" << width->Value() << ";";
  if (height != nullptr)
    out << "height:" << height->Value() << ";";
  if (x != nullptr)
    out << "left:" << x->Value() << ";";
  if (y != nullptr)
    out << "top:" << y->Value() << ";";
  out << "\"";

  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%"></rect></svg>)";
  out << "</div>";
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out, Context &context) {
  common::XmlUtil::visitNodeChildren(in, [&](const tinyxml2::XMLNode &n) {
    if (n.ToText() != nullptr)
      TextTranslator(*n.ToText(), out, context);
    if (n.ToElement() != nullptr)
      ElementTranslator(*n.ToElement(), out, context);
  });
}

void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
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

  const std::string element = in.Name();
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
    if (it != substitution.end()) {
      out << "</" << it->second << ">";
    }
  }
}
} // namespace

void ContentTranslator::html(const tinyxml2::XMLElement &in, Context &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace odf
} // namespace odr
