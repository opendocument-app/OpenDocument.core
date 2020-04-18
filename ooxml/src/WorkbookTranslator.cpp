#include <Context.h>
#include <WorkbookTranslator.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/StringUtil.h>
#include <common/XmlUtil.h>
#include <glog/logging.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <string>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>

namespace odr {
namespace ooxml {

namespace {
void FontsTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &) {
  std::uint32_t i = 0;
  common::XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
    out << ".font-" << i << " {";

    const tinyxml2::XMLElement *name = e.FirstChildElement("name");
    if (name != nullptr)
      out << "font-family: " << name->FindAttribute("val")->Value() << ";";

    const tinyxml2::XMLElement *size = e.FirstChildElement("sz");
    if (size != nullptr)
      out << "font-size: " << size->FindAttribute("val")->Value() << "pt;";

    const tinyxml2::XMLElement *color = e.FirstChildElement("color");
    if ((color != nullptr) && (color->FindAttribute("rgb") != nullptr))
      out << "color: #"
          << std::string(color->FindAttribute("rgb")->Value()).substr(2) << ";";

    // TODO
    // <u val="single" /> underline?
    // <b val="true" /> bold?
    // <vertAlign val="superscript" />

    out << "} ";

    ++i;
  });
}

void FillsTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &) {
  std::uint32_t i = 0;
  common::XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
    out << ".fill-" << i << " {";

    const auto patternFill = e.FirstChildElement("patternFill");
    if (patternFill != nullptr) {
      const auto bgColor = patternFill->FirstChildElement("bgColor");
      if (bgColor != nullptr)
        out << "background-color: #"
            << std::string(bgColor->FindAttribute("rgb")->Value()).substr(2)
            << ";";
    }

    out << "} ";

    ++i;
  });
}

void BordersTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       Context &) {
  std::uint32_t i = 0;
  common::XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &) {
    out << ".border-" << i << " {";
    // TODO
    out << "} ";

    ++i;
  });
}

void CellXfsTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       Context &context) {
  std::uint32_t i = 0;
  common::XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
    const std::string name = "cellxf-" + std::to_string(i);

    const tinyxml2::XMLAttribute *fontId = e.FindAttribute("fontId");
    const tinyxml2::XMLAttribute *applyFont = e.FindAttribute("applyFont");
    if (applyFont != nullptr && (std::strcmp(applyFont->Value(), "true") == 0 ||
                                 std::strcmp(applyFont->Value(), "1") == 0))
      context.styleDependencies[name].push_back(std::string("font-") +
                                                fontId->Value());

    const tinyxml2::XMLAttribute *fillId = e.FindAttribute("fillId");
    if (fillId != nullptr)
      context.styleDependencies[name].push_back(std::string("fill-") +
                                                fillId->Value());

    const tinyxml2::XMLAttribute *borderId = e.FindAttribute("borderId");
    const tinyxml2::XMLAttribute *applyBorder = e.FindAttribute("applyBorder");
    if (applyBorder != nullptr &&
        (std::strcmp(applyBorder->Value(), "true") == 0 ||
         std::strcmp(applyBorder->Value(), "1") == 0))
      context.styleDependencies[name].push_back(std::string("border-") +
                                                borderId->Value());

    out << "." << name << " {";

    const tinyxml2::XMLAttribute *applyAlignment =
        e.FindAttribute("applyAlignment");
    if (applyAlignment != nullptr &&
        (std::strcmp(applyAlignment->Value(), "true") == 0 ||
         std::strcmp(applyAlignment->Value(), "1") == 0)) {
      const auto *alignment = e.FirstChildElement("alignment");
      out << "text-align: " << alignment->FindAttribute("horizontal")->Value()
          << ";";
      // TODO vertical alignment
      // <alignment horizontal="left" vertical="bottom" textRotation="0"
      // wrapText="false" indent="0" shrinkToFit="false" />
    }

    // TODO
    // <protection locked="true" hidden="false" />

    out << "} ";

    ++i;
  });
}
} // namespace

void WorkbookTranslator::css(const tinyxml2::XMLElement &in, Context &context) {
  std::ostream &out = *context.output;

  const tinyxml2::XMLElement *fonts = in.FirstChildElement("fonts");
  if (fonts != nullptr)
    FontsTranslator(*fonts, out, context);

  const tinyxml2::XMLElement *fills = in.FirstChildElement("fills");
  if (fills != nullptr)
    FillsTranslator(*fills, out, context);

  const tinyxml2::XMLElement *borders = in.FirstChildElement("borders");
  if (borders != nullptr)
    BordersTranslator(*borders, out, context);

  const tinyxml2::XMLElement *cellXfs = in.FirstChildElement("cellXfs");
  if (cellXfs != nullptr)
    CellXfsTranslator(*cellXfs, out, context);
}

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

void StyleAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                              Context &) {
  const std::string prefix = in.Name();

  const tinyxml2::XMLAttribute *width = in.FindAttribute("width");
  const tinyxml2::XMLAttribute *ht = in.FindAttribute("ht");
  if ((width != nullptr) || (ht != nullptr)) {
    out << " style=\"";
    if (width != nullptr)
      out << "width:" << (width->Int64Value()) << "in;";
    if (ht != nullptr)
      out << "height:" << (ht->Int64Value()) << "pt;";
    out << "\"";
  }
}

void ElementAttributeTranslator(const tinyxml2::XMLElement &in,
                                std::ostream &out, Context &context) {
  const auto s = in.FindAttribute("s");
  if (s != nullptr) {
    const std::string name = std::string("cellxf-") + s->Value();
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

    out << "\"";
  }

  StyleAttributeTranslator(in, out, context);
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out, Context &context);
void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       Context &context);

void TableTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &context) {
  // TODO context.config->tableLimitByDimensions
  context.tableRange = {
      {context.config->tableOffsetRows, context.config->tableOffsetCols},
      context.config->tableLimitRows,
      context.config->tableLimitCols};
  context.tableCursor = {};

  out << R"(<table border="0" cellspacing="0" cellpadding="0")";
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";
}

void TableColTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &context) {
  // TODO if min/max is unordered we have a problem here; fail fast in that case

  const auto min = in.Unsigned64Attribute("min", 1);
  const auto max = in.Unsigned64Attribute("max", 1);
  const auto repeated = max - min + 1;

  for (std::uint32_t i = 0; i < repeated; ++i) {
    if (context.tableCursor.col() >= context.tableRange.to().col())
      break;
    if (context.tableCursor.col() >= context.tableRange.from().col()) {
      out << "<col";
      ElementAttributeTranslator(in, out, context);
      out << ">";
    }
    context.tableCursor.addCol();
  }
}

void TableRowTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &context) {
  const auto rowIndex = in.FindAttribute("r")->Unsigned64Value() - 1;

  while (rowIndex > context.tableCursor.row()) {
    if (context.tableCursor.row() >= context.tableRange.to().row())
      return;
    if (context.tableCursor.row() >= context.tableRange.from().row()) {
      // TODO insert empty proper rows
      out << "<tr></tr>";
    }
    context.tableCursor.addRow();
  }

  context.tableCursor.addRow(0); // TODO hacky
  if (context.tableCursor.row() >= context.tableRange.to().row())
    return;
  if (context.tableCursor.row() >= context.tableRange.from().row()) {
    out << "<tr";
    ElementAttributeTranslator(in, out, context);
    out << ">";
    ElementChildrenTranslator(in, out, context);
    out << "</tr>";
  }
  context.tableCursor.addRow();
}

void TableCellTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &context) {
  const common::TablePosition cellIndex(in.FindAttribute("r")->Value());

  while (cellIndex.col() > context.tableCursor.col()) {
    if (context.tableCursor.col() >= context.tableRange.to().col())
      return;
    if (context.tableCursor.col() >= context.tableRange.from().col()) {
      out << "<td></td>";
    }
    context.tableCursor.addCell();
  }

  out << "<td";
  ElementAttributeTranslator(in, out, context);
  out << ">";

  const auto t = in.FindAttribute("t");
  if (t != nullptr) {
    if (std::strcmp(t->Value(), "s") == 0) {
      const auto sharedStringIndex = in.FirstChildElement("v")->IntText(-1);
      if (sharedStringIndex >= 0) {
        const tinyxml2::XMLElement &replacement =
            *context.sharedStrings[sharedStringIndex];
        ElementChildrenTranslator(replacement, out, context);
      } else {
        DLOG(INFO) << "undefined behaviour: shared string not found";
      }
    } else if ((std::strcmp(t->Value(), "str") == 0) ||
               (std::strcmp(t->Value(), "inlineStr") == 0) ||
               (std::strcmp(t->Value(), "n") == 0)) {
      ElementChildrenTranslator(in, out, context);
    } else {
      DLOG(INFO) << "undefined behaviour: t=" << t->Value();
    }
  } else {
    // TODO empty cell?
  }

  out << "</td>";
  context.tableCursor.addCell();
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out, Context &context) {
  common::XmlUtil::visitNodeChildren(in, [&](const tinyxml2::XMLNode &n) {
    if (n.ToText() != nullptr)
      TextTranslator(*n.ToText(), out, context);
    else if (n.ToElement() != nullptr)
      ElementTranslator(*n.ToElement(), out, context);
  });
}

void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       Context &context) {
  static std::unordered_map<std::string, const char *> substitution{
      {"cols", "colgroup"},
  };
  static std::unordered_set<std::string> skippers{
      "headerFooter",
      "f", // TODO translate formula and hide
  };

  const std::string element = in.Name();
  if (skippers.find(element) != skippers.end())
    return;

  if (element == "worksheet")
    TableTranslator(in, out, context);
  else if (element == "col")
    TableColTranslator(in, out, context);
  else if (element == "row")
    TableRowTranslator(in, out, context);
  else if (element == "c")
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

void WorkbookTranslator::html(const tinyxml2::XMLElement &in,
                              Context &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace ooxml
} // namespace odr
