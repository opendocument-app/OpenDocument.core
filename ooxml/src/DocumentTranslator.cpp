#include <Context.h>
#include <DocumentTranslator.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/StringUtil.h>
#include <crypto/CryptoUtil.h>
#include <cstring>
#include <glog/logging.h>
#include <odr/Config.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace odr {
namespace ooxml {

namespace {
void AlignmentTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &) {
  out << "text-align:" << in.attribute("w:val").as_string() << ";";
}

void FontTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto fontAttr = in.attribute("w:cs");
  if (!fontAttr)
    return;
  out << "font-family:" << fontAttr.as_string() << ";";
}

void FontSizeTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &) {
  const auto sizeAttr = in.attribute("w:val");
  if (!sizeAttr)
    return;
  const double size = sizeAttr.as_int() / 2.0;
  out << "font-size:" << size << "pt;";
}

void BoldTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto valAttr = in.attribute("w:val");
  if (valAttr)
    return;
  out << "font-weight:bold;";
}

void ItalicTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto valAttr = in.attribute("w:val");
  if (valAttr)
    return;
  out << "font-style:italic;";
}

void UnderlineTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &) {
  const auto valAttr = in.attribute("w:val");
  if (std::strcmp(valAttr.as_string(), "single") == 0)
    out << "text-decoration:underline;";
  // TODO wont work with StrikeThroughTranslator
}

void StrikeThroughTranslator(const pugi::xml_node &in, std::ostream &out,
                             Context &) {
  // TODO wont work with UnderlineTranslator

  const auto valAttr = in.attribute("w:val");
  if (valAttr && (std::strcmp(valAttr.as_string(), "false") == 0))
    return;
  out << "text-decoration:line-through;";
}

void ShadowTranslator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "text-shadow:1pt 1pt;";
}

void ColorTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto valAttr = in.attribute("w:val");
  if (std::strcmp(valAttr.as_string(), "auto") == 0)
    return;
  if (std::strlen(valAttr.as_string()) == 6)
    out << "color:#" << valAttr.as_string() << ";";
  else
    out << "color:" << valAttr.as_string() << ";";
}

void HighlightTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &) {
  const auto valAttr = in.attribute("w:val");
  if (std::strcmp(valAttr.as_string(), "auto") == 0)
    return;
  if (std::strlen(valAttr.as_string()) == 6)
    out << "background-color:#" << valAttr.as_string() << ";";
  else
    out << "background-color:" << valAttr.as_string() << ";";
}

void IndentationTranslator(const pugi::xml_node &in, std::ostream &out,
                           Context &) {
  const auto leftAttr = in.attribute("w:left");
  if (leftAttr)
    out << "margin-left:" << leftAttr.as_float() / 1440.0f << "in;";

  const auto rightAttr = in.attribute("w:right");
  if (rightAttr)
    out << "margin-right:" << rightAttr.as_float() / 1440.0f << "in;";
}

void TableCellWidthTranslator(const pugi::xml_node &in, std::ostream &out,
                              Context &) {
  const auto widthAttr = in.attribute("w:w");
  const auto typeAttr = in.attribute("w:type");
  if (!widthAttr)
    return;
  float width = widthAttr.as_float();
  if (typeAttr && std::strcmp(typeAttr.as_string(), "dxa") == 0)
    width /= 1440.0f;
  out << "width:" << width << "in;";
}

void TableCellBorderTranslator(const pugi::xml_node &in, std::ostream &out,
                               Context &) {
  auto translator = [&](const char *name, const pugi::xml_node &e) {
    out << name << ":";

    const auto val = e.attribute("w:val").as_string();
    if (std::strcmp(val, "nil") == 0) {
      out << "none";
    } else {
      const float sizePt = e.attribute("w:sz").as_float() / 2.0f;
      out << sizePt << "pt ";

      out << "solid ";

      const auto colorAttr = e.attribute("w:color");
      if (std::strlen(colorAttr.as_string()) == 6)
        out << "#" << colorAttr.as_string();
      else
        out << colorAttr.as_string();
    }

    out << ";";
  };

  if (const auto top = in.child("w:top"); top)
    translator("border-top", top);

  if (const auto top = in.child("w:left"); top)
    translator("border-left", top);

  if (const auto top = in.child("w:bottom"); top)
    translator("border-bottom", top);

  if (const auto top = in.child("w:right"); top)
    translator("border-right", top);
}

void translateStyleInline(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  for (auto &&e : in.children()) {
    const std::string element = e.name();

    if (element == "w:jc")
      AlignmentTranslator(e, out, context);
    else if (element == "w:rFonts")
      FontTranslator(e, out, context);
    else if (element == "w:sz")
      FontSizeTranslator(e, out, context);
    else if (element == "w:b")
      BoldTranslator(e, out, context);
    else if (element == "w:i")
      ItalicTranslator(e, out, context);
    else if (element == "w:u")
      UnderlineTranslator(e, out, context);
    else if (element == "w:strike")
      StrikeThroughTranslator(e, out, context);
    else if (element == "w:shadow")
      ShadowTranslator(e, out, context);
    else if (element == "w:color")
      ColorTranslator(e, out, context);
    else if (element == "w:highlight")
      HighlightTranslator(e, out, context);
    else if (element == "w:ind")
      IndentationTranslator(e, out, context);
    else if (element == "w:tcW")
      TableCellWidthTranslator(e, out, context);
    else if (element == "w:tcBorders")
      TableCellBorderTranslator(e, out, context);
  }
}

void StyleClassTranslator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  std::string name = "unknown";
  if (const auto nameAttr = in.attribute("w:styleId"); nameAttr) {
    name = nameAttr.value();
  } else {
    LOG(WARNING) << "no name attribute " << in.name();
  }

  std::string type = "unknown";
  if (const auto typeAttr = in.attribute("w:type"); typeAttr) {
    type = typeAttr.value();
  } else {
    LOG(WARNING) << "no type attribute " << in.name();
  }

  /*
  const char *parentStyleName;
  if (in.QueryStringAttribute("style:parent-style-name", &parentStyleName) ==
  tinyxml2::XML_SUCCESS) {
      context.odStyleDependencies[styleName].push_back(parentStyleName);
  }
   */

  out << "." << name << " {";

  if (const auto pPr = in.child("w:pPr"); pPr)
    translateStyleInline(pPr, out, context);

  if (const auto rPr = in.child("w:rPr"); rPr)
    translateStyleInline(rPr, out, context);

  out << "}\n";
}
} // namespace

void DocumentTranslator::css(const pugi::xml_node &in, Context &context) {
  for (auto &&e : in.children()) {
    const std::string element = e.name();

    if (element == "w:style")
      StyleClassTranslator(e, *context.output, context);
    // else if (element == "w:docDefaults") DefaultStyleTranslator(e,
    // *context.output, context);
  }
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
                              Context &context) {
  const std::string prefix = in.name();

  const pugi::xml_node style =
      in.child((prefix + "Pr").c_str()).child((prefix + "Style").c_str());
  if (style)
    *context.output << " class=\"" << style.attribute("w:val").as_string()
                    << "\"";

  const pugi::xml_node inlineStyle = in.child((prefix + "Pr").c_str());
  if (inlineStyle && inlineStyle.first_child()) {
    out << " style=\"";
    translateStyleInline(inlineStyle, out, context);
    out << "\"";
  }
}

void ElementAttributeTranslator(const pugi::xml_node &in, std::ostream &out,
                                Context &context) {
  StyleAttributeTranslator(in, out, context);
}

void ElementChildrenTranslator(const pugi::xml_node &in, std::ostream &out,
                               Context &context);
void ElementTranslator(const pugi::xml_node &in, std::ostream &out,
                       Context &context);

void TabTranslator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "\t";
}

void ParagraphTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  const pugi::xml_node num = in.child("w:pPr").child("w:numPr");
  int listingLevel;
  if (num) {
    listingLevel = num.child("w:ilvl").attribute("w:val").as_int();
    for (int i = 0; i <= listingLevel; ++i) {
      out << "<ul>";
    }
    out << "<li>";
  }

  out << "<p";
  ElementAttributeTranslator(in, out, context);
  out << ">";

  bool empty = true;
  for (auto &&e1 : in) {
    for (auto &&e2 : e1) {
      if (common::StringUtil::endsWith(e1.name(), "Pr"))
        ;
      else if (std::strcmp(e1.name(), "w:r") != 0)
        empty = false;
      else if (!common::StringUtil::endsWith(e2.name(), "Pr"))
        empty = false;
    }
  }

  if (empty)
    out << "<br/>";
  else
    ElementChildrenTranslator(in, out, context);

  out << "</p>";

  if (num) {
    out << "</li>";
    for (int i = 0; i <= listingLevel; ++i) {
      out << "</ul>";
    }
  }
}

void SpanTranslator(const pugi::xml_node &in, std::ostream &out,
                    Context &context) {
  out << "<span";
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</span>";
}

void HyperlinkTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  out << "<a";

  if (const auto anchorAttr = in.attribute("w:anchor"); anchorAttr)
    out << " href=\"#" << anchorAttr.as_string() << R"(" target="_self")";
  else if (const auto rIdAttr = in.attribute("r:id"); rIdAttr)
    out << " href=\"" << context.relations[rIdAttr.as_string()] << "\"";

  ElementAttributeTranslator(in, out, context);

  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</a>";
}

void BookmarkTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &) {
  if (const auto nameAttr = in.attribute("w:name"); nameAttr)
    out << "<a id=\"" << nameAttr.as_string() << "\"/>";
}

void TableTranslator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << R"(<table border="0" cellspacing="0" cellpadding="0")";
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";
}

void DrawingsTranslator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  // ooxml is using amazing units
  // https://startbigthinksmall.wordpress.com/2010/01/04/points-inches-and-emus-measuring-units-in-office-open-xml/

  const auto child = in.first_child();
  if (!child)
    return;
  const auto graphic = child.child("a:graphic");
  if (!graphic)
    return;
  // TODO handle something other than inline

  out << "<div";

  if (const auto sizeEle = child.child("wp:extent"); sizeEle) {
    const float widthIn = sizeEle.attribute("cx").as_float() / 914400.0f;
    const float heightIn = sizeEle.attribute("cy").as_float() / 914400.0f;
    out << " style=\"width:" << widthIn << "in;height:" << heightIn << "in;\"";
  }

  out << ">";
  ElementTranslator(graphic, out, context);
  out << "</div>";
}

void ImageTranslator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << "<img style=\"width:100%;height:100%\"";

  const pugi::xml_node ref = in.child("pic:blipFill").child("a:blip");
  if (!ref || !ref.attribute("r:embed")) {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  } else {
    const char *rIdAttr = ref.attribute("r:embed").as_string();
    const auto path = access::Path("word").join(context.relations[rIdAttr]);
    out << " alt=\"Error: image not found or unsupported: " << path << "\"";
    out << " src=\"";
    std::string image = access::StreamUtil::read(*context.storage->read(path));
    // hacky image/jpg working according to tom
    out << "data:image/jpg;base64, ";
    out << crypto::Util::base64Encode(image);
    out << "\"";
  }

  out << "></img>";
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
      {"w:tr", "tr"},
      {"w:tc", "td"},
  };
  static std::unordered_set<std::string> skippers{
      "w:instrText",
  };

  const std::string element = in.name();
  if (skippers.find(element) != skippers.end())
    return;

  if (element == "w:tab")
    TabTranslator(in, out, context);
  else if (element == "w:p")
    ParagraphTranslator(in, out, context);
  else if (element == "w:r")
    SpanTranslator(in, out, context);
  else if (element == "w:hyperlink")
    HyperlinkTranslator(in, out, context);
  else if (element == "w:bookmarkStart")
    BookmarkTranslator(in, out, context);
  else if (element == "w:tbl")
    TableTranslator(in, out, context);
  else if (element == "w:drawing")
    DrawingsTranslator(in, out, context);
  else if (element == "pic:pic")
    ImageTranslator(in, out, context);
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

void DocumentTranslator::html(const pugi::xml_node &in, Context &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace ooxml
} // namespace odr
