#include <Context.h>
#include <WorkbookTranslator.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/StringUtil.h>
#include <common/XmlUtil.h>
#include <cstring>
#include <glog/logging.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace odr {
namespace ooxml {

namespace {
void FontsTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    out << ".font-" << i << " {";

    if (const auto name = e.child("name"); name)
      out << "font-family: " << name.attribute("val").as_string() << ";";

    if (const auto size = e.child("sz"); size)
      out << "font-size: " << size.attribute("val").as_string() << "pt;";

    if (const auto color = e.child("color"); color && color.attribute("rgb"))
      out << "color: #"
          << std::string(color.attribute("rgb").as_string()).substr(2) << ";";

    // TODO
    // <u val="single" /> underline?
    // <b val="true" /> bold?
    // <vertAlign val="superscript" />

    out << "} ";

    ++i;
  }
}

void FillsTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    out << ".fill-" << i << " {";

    if (const auto patternFill = e.child("patternFill"); patternFill) {
      if (const auto bgColor = patternFill.child("bgColor"); bgColor)
        out << "background-color: #"
            << std::string(bgColor.attribute("rgb").as_string()).substr(2)
            << ";";
    }

    out << "} ";

    ++i;
  }
}

void BordersTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    out << ".border-" << i << " {";
    // TODO
    out << "} ";

    ++i;
  }
}

void CellXfsTranslator(const pugi::xml_node &in, std::ostream &out,
                       Context &context) {
  std::uint32_t i = 0;
  for (auto &&e : in.children()) {
    const std::string name = "cellxf-" + std::to_string(i);

    if (const auto applyFont = e.attribute("applyFont");
        applyFont && (std::strcmp(applyFont.as_string(), "true") == 0 ||
                      std::strcmp(applyFont.as_string(), "1") == 0))
      context.styleDependencies[name].push_back(
          std::string("font-") + e.attribute("fontId").as_string());

    if (const auto fillId = e.attribute("fillId"); fillId)
      context.styleDependencies[name].push_back(std::string("fill-") +
                                                fillId.as_string());

    if (const auto applyBorder = e.attribute("fillId");
        applyBorder &&
        (std::strcmp(applyBorder.as_string(), "true") == 0 ||
         std::strcmp(applyBorder.as_string(), "1") == 0))
      context.styleDependencies[name].push_back(
          std::string("border-") + e.attribute("borderId").as_string());

    out << "." << name << " {";

    if (const auto applyAlignment = e.attribute("applyAlignment");
        applyAlignment &&
        (std::strcmp(applyAlignment.as_string(), "true") == 0 ||
         std::strcmp(applyAlignment.as_string(), "1") == 0)) {
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

void WorkbookTranslator::css(const pugi::xml_node &in, Context &context) {
  std::ostream &out = *context.output;

  if (const auto fonts = in.child("fonts"); fonts)
    FontsTranslator(fonts, out, context);

  if (const auto fills = in.child("fills"); fills)
    FillsTranslator(fills, out, context);

  if (const auto borders = in.child("borders"); borders)
    BordersTranslator(borders, out, context);

  if (const auto cellXfs = in.child("cellXfs"); cellXfs)
    CellXfsTranslator(cellXfs, out, context);
}

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

void StyleAttributeTranslator(const pugi::xml_node &in, std::ostream &out,
                              Context &) {
  const std::string prefix = in.name();

  const auto width = in.attribute("width");
  const auto ht = in.attribute("ht");
  if (width || ht) {
    out << " style=\"";
    if (width)
      out << "width:" << width.as_float() << "in;";
    if (ht)
      out << "height:" << ht.as_float() << "pt;";
    out << "\"";
  }
}

void ElementAttributeTranslator(const pugi::xml_node &in, std::ostream &out,
                                Context &context) {
  if (const auto s = in.attribute("s"); s) {
    const std::string name = std::string("cellxf-") + s.as_string();
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

void ElementChildrenTranslator(const pugi::xml_node &in, std::ostream &out,
                               Context &context);
void ElementTranslator(const pugi::xml_node &in, std::ostream &out,
                       Context &context);

void TableTranslator(const pugi::xml_node &in, std::ostream &out,
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

void TableColTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  // TODO if min/max is unordered we have a problem here; fail fast in that case

  const auto min = in.attribute("min").as_uint(1);
  const auto max = in.attribute("max").as_uint(1);
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

void TableRowTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  const auto rowIndex = in.attribute("r").as_uint() - 1;

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

void TableCellTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  const common::TablePosition cellIndex(in.attribute("r").as_string());

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

  if (const auto t = in.attribute("t"); t) {
    if (std::strcmp(t.as_string(), "s") == 0) {
      const auto sharedStringIndex = in.child("v").text().as_int(-1);
      if (sharedStringIndex >= 0) {
        const pugi::xml_node &replacement =
            context.sharedStrings[sharedStringIndex];
        ElementChildrenTranslator(replacement, out, context);
      } else {
        DLOG(INFO) << "undefined behaviour: shared string not found";
      }
    } else if ((std::strcmp(t.as_string(), "str") == 0) ||
               (std::strcmp(t.as_string(), "inlineStr") == 0) ||
               (std::strcmp(t.as_string(), "n") == 0)) {
      ElementChildrenTranslator(in, out, context);
    } else {
      DLOG(INFO) << "undefined behaviour: t=" << t.as_string();
    }
  } else {
    // TODO empty cell?
  }

  out << "</td>";
  context.tableCursor.addCell();
}

void ElementChildrenTranslator(const pugi::xml_node &in, std::ostream &out,
                               Context &context) {
  for (auto &&n : in) {
    if (n.text())
      TextTranslator(n.text(), out, context);
    else if (n)
      ElementTranslator(n, out, context);
  }
}

void ElementTranslator(const pugi::xml_node &in, std::ostream &out,
                       Context &context) {
  static std::unordered_map<std::string, const char *> substitution{
      {"cols", "colgroup"},
  };
  static std::unordered_set<std::string> skippers{
      "headerFooter",
      "f", // TODO translate formula and hide
  };

  const std::string element = in.name();
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

void WorkbookTranslator::html(const pugi::xml_node &in, Context &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace ooxml
} // namespace odr
