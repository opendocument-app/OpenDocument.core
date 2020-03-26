#include <access/Path.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/StringUtil.h>
#include <common/TranslationContext.h>
#include <common/XmlUtil.h>
#include <crypto/CryptoUtil.h>
#include <glog/logging.h>
#include <odf/OpenDocumentContentTranslator.h>
#include <odf/OpenDocumentStyleTranslator.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <string>
#include <svm/Svm2Svg.h>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>

namespace odr {

namespace {
void TextTranslator(const tinyxml2::XMLText &in, std::ostream &out,
                    common::TranslationContext &context) {
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

void StyleAttributeTranslator(const std::string &name, std::ostream &out,
                              common::TranslationContext &context) {
  out << " class=\"";
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

  // TODO draw:master-page-name

  out << "\"";
}

void StyleAttributeTranslator(const tinyxml2::XMLAttribute &in,
                              std::ostream &out,
                              common::TranslationContext &context) {
  const std::string name =
      OpenDocumentStyleTranslator::escapeStyleName(in.Value());
  StyleAttributeTranslator(name, out, context);
}

void AttributeTranslator(const tinyxml2::XMLAttribute &in, std::ostream &out,
                         common::TranslationContext &context) {
  static std::unordered_set<std::string> styleAttributes{
      "text:style-name",
      "table:style-name",
      "draw:style-name",
      "presentation:style-name",
  };

  const std::string element = in.Name();
  if (styleAttributes.find(element) != styleAttributes.end()) {
    StyleAttributeTranslator(in, out, context);
  }
}

void ElementAttributeTranslator(const tinyxml2::XMLElement &in,
                                std::ostream &out,
                                common::TranslationContext &context) {
  common::XmlUtil::visitElementAttributes(
      in, [&](const tinyxml2::XMLAttribute &a) {
        AttributeTranslator(a, out, context);
      });
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out,
                               common::TranslationContext &context);
void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       common::TranslationContext &context);

void ParagraphTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         common::TranslationContext &context) {
  out << "<p";
  ElementAttributeTranslator(in, out, context);
  out << ">";

  if (in.FirstChild() == nullptr)
    out << "<br/>";
  else
    ElementChildrenTranslator(in, out, context);

  out << "</p>";
}

void SpaceTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     common::TranslationContext &) {
  const auto count = in.Unsigned64Attribute("text:c", 1);
  if (count <= 0) {
    return;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    // TODO: use "&nbsp;"?
    out << " ";
  }
}

void TabTranslator(const tinyxml2::XMLElement &, std::ostream &out,
                   common::TranslationContext &) {
  // TODO: use "&emsp;"?
  out << "\t";
}

void LinkTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                    common::TranslationContext &context) {
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
                        common::TranslationContext &context) {
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
                     common::TranslationContext &context) {
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
                     common::TranslationContext &context) {
  out << "<img style=\"width:100%;height:100%\"";

  const auto href = in.FindAttribute("xlink:href");
  if (href == nullptr) {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  } else {
    const std::string &path = href->Value();
    out << " alt=\"Error: image not found or unsupported: " << path << "\"";
    out << " src=\"";
    if (!context.storage->isFile(path)) {
      out << path;
    } else {
      std::string image = access::StreamUtil::read(*context.storage->read(path));
      if ((path.find("ObjectReplacements", 0) != std::string::npos) ||
          (path.find(".svm", 0) != std::string::npos)) {
        std::istringstream svmIn(image);
        std::ostringstream svgOut;
        svm::Svm2Svg::translate(svmIn, svgOut);
        image = svgOut.str();
        out << "data:image/svg+xml;base64, ";
      } else {
        // hacky image/jpg working according to tom
        out << "data:image/jpg;base64, ";
      }
      out << crypto::CryptoUtil::base64Encode(image);
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
                     common::TranslationContext &context) {
  context.currentTableRowStart = context.config->tableOffsetRows;
  context.currentTableRowEnd =
      context.currentTableRowStart + context.config->tableLimitRows;
  context.currentTableColStart = context.config->tableOffsetCols;
  context.currentTableColEnd =
      context.currentTableColStart + context.config->tableLimitCols;
  // TODO remove file check; add simple table translator for odt/odp
  if ((context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) &&
      context.config->tableLimitByDimensions) {
    context.currentTableRowEnd =
        std::min(context.currentTableRowEnd,
                 context.currentTableRowStart +
                     context.meta->entries[context.currentEntry].rowCount);
    context.currentTableColEnd =
        std::min(context.currentTableColEnd,
                 context.currentTableColStart +
                     context.meta->entries[context.currentEntry].columnCount);
  }
  context.tableCursor = {};
  context.odDefaultCellStyles.clear();

  out << "<table";
  ElementAttributeTranslator(in, out, context);
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";

  ++context.currentEntry;
}

void TableColumnTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                           common::TranslationContext &context) {
  const auto repeated =
      in.Unsigned64Attribute("table:number-columns-repeated", 1);
  const auto defaultCellStyleAttribute =
      in.FindAttribute("table:default-cell-style-name");
  // TODO we could use span instead
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.getCol() >= context.currentTableColEnd)
      break;
    if (context.tableCursor.getCol() >= context.currentTableColStart) {
      if (defaultCellStyleAttribute != nullptr) {
        context.odDefaultCellStyles[context.tableCursor.getCol()] =
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
                        common::TranslationContext &context) {
  const auto repeated = in.Unsigned64Attribute("table:number-rows-repeated", 1);
  context.tableCursor.addRow(0); // TODO hacky
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.getRow() >= context.currentTableRowEnd)
      break;
    if (context.tableCursor.getRow() >= context.currentTableRowStart) {
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
                         common::TranslationContext &context) {
  const auto repeated =
      in.Unsigned64Attribute("table:number-columns-repeated", 1);
  const auto colspan =
      in.Unsigned64Attribute("table:number-columns-spanned", 1);
  const auto rowspan = in.Unsigned64Attribute("table:number-rows-spanned", 1);
  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.getCol() >= context.currentTableColEnd)
      break;
    if (context.tableCursor.getCol() >= context.currentTableColStart) {
      out << "<td";
      if (in.FindAttribute("table:style-name") == nullptr) {
        const auto it =
            context.odDefaultCellStyles.find(context.tableCursor.getCol());
        if (it != context.odDefaultCellStyles.end())
          StyleAttributeTranslator(it->second, out, context);
      }
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

void ElementChildrenTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out,
                               common::TranslationContext &context) {
  common::XmlUtil::visitNodeChildren(in, [&](const tinyxml2::XMLNode &n) {
    if (n.ToText() != nullptr)
      TextTranslator(*n.ToText(), out, context);
    if (n.ToElement() != nullptr)
      ElementTranslator(*n.ToElement(), out, context);
  });
}

void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       common::TranslationContext &context) {
  static std::unordered_map<std::string, const char *> substitution{
      {"text:span", "span"},    {"text:line-break", "br"}, {"text:list", "ul"},
      {"text:list-item", "li"}, {"draw:page", "div"},
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

void OpenDocumentContentTranslator::translate(
    const tinyxml2::XMLElement &in, common::TranslationContext &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace odr
