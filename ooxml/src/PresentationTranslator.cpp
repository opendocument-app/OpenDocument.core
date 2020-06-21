#include <Context.h>
#include <PresentationTranslator.h>
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
void XfrmTranslator(const pugi::xml_node &in, std::ostream &out, Context &) {
  if (const auto offEle = in.child("a:off"); offEle) {
    const float xIn = offEle.attribute("x").as_float() / 914400.0f;
    const float yIn = offEle.attribute("y").as_float() / 914400.0f;
    out << "position:absolute;";
    out << "left:" << xIn << "in;";
    out << "top:" << yIn << "in;";
  }

  if (const auto extEle = in.child("a:ext"); extEle) {
    const float cxIn = extEle.attribute("cx").as_float() / 914400.0f;
    const float cyIn = extEle.attribute("cy").as_float() / 914400.0f;
    out << "width:" << cxIn << "in;";
    out << "height:" << cyIn << "in;";
  }
}

void BorderTranslator(const std::string &property, const pugi::xml_node &in,
                      std::ostream &out, Context &) {
  const auto wAttr = in.attribute("w");
  if (!wAttr)
    return;

  const auto colorEle = in.child("a:solidFill").child("a:srgbClr");
  if (!colorEle)
    return;

  const auto valAttr = colorEle.attribute("val");
  if (!valAttr)
    return;

  out << property << ":" << wAttr.as_float() / 14400.0f << "pt solid ";
  if (std::strlen(valAttr.as_string()) == 6)
    out << "#";
  out << valAttr.as_string();
  out << ";";
}

void BackgroundColorTranslator(const pugi::xml_node &in, std::ostream &out,
                               Context &) {
  const auto colorEle = in.child("a:srgbClr");
  if (!colorEle)
    return;

  const auto valAttr = colorEle.attribute("val");
  if (!valAttr)
    return;

  out << "background-color:";
  if (std::strlen(valAttr.as_string()) == 6)
    out << "#";
  out << valAttr.as_string();
  out << ";";
}

void MarginAttributesTranslator(const pugi::xml_node &in, std::ostream &out,
                                Context &) {
  const auto marLAttr = in.attribute("marL");
  if (!marLAttr) {
    float marLIn = marLAttr.as_float() / 914400.0f;
    out << "margin-left:" << marLIn << "in;";
  }

  const auto marRAttr = in.attribute("marR");
  if (!marRAttr) {
    float marRIn = marRAttr.as_float() / 914400.0f;
    out << "margin-right:" << marRIn << "in;";
  }
}

void TableCellPropertyTranslator(const pugi::xml_node &in, std::ostream &out,
                                 Context &context) {
  MarginAttributesTranslator(in, out, context);

  for (auto &&e : in) {
    const std::string element = e.name();

    if (element == "a:lnL")
      BorderTranslator("border-left", e, out, context);
    else if (element == "a:lnR")
      BorderTranslator("border-right", e, out, context);
    else if (element == "a:lnT")
      BorderTranslator("border-top", e, out, context);
    else if (element == "a:lnB")
      BorderTranslator("border-bottom", e, out, context);
    else if (element == "a:solidFill")
      BackgroundColorTranslator(e, out, context);
  }
}

void DefaultPropertyTransaltor(const pugi::xml_node &in, std::ostream &out,
                               Context &context) {
  MarginAttributesTranslator(in, out, context);

  const auto szAttr = in.attribute("sz");
  if (szAttr) {
    float szPt = szAttr.as_float() / 100.0f;
    out << "font-size:" << szPt << "pt;";
  }

  const auto algnAttr = in.attribute("algn");
  if (algnAttr) {
    out << "text-align:";
    if (std::strcmp(algnAttr.as_string(), "l") == 0)
      out << "left";
    if (std::strcmp(algnAttr.as_string(), "ctr") == 0)
      out << "center";
    if (std::strcmp(algnAttr.as_string(), "r") == 0)
      out << "right";
    out << ";";
  }

  for (auto &&e : in) {
    if (std::strcmp(e.name(), "a:xfrm") == 0)
      XfrmTranslator(e, out, context);
  }
}
} // namespace

void PresentationTranslator::css(const pugi::xml_node &, Context &) {}

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
  const auto pPr = in.child("a:pPr");
  const auto rPr = in.child("a:rPr");
  const auto spPr = in.child("p:spPr");
  const auto tcPr = in.child("a:tcPr");
  const auto endParaRPr = in.child("a:endParaRPr");
  const auto xfrmPr = in.child("p:xfrm");
  const auto wAttr = in.attribute("w");
  const auto hAttr = in.attribute("h");
  if ((pPr && pPr.first_child()) || (rPr && rPr.first_child()) ||
      (spPr && spPr.first_child()) || (tcPr && tcPr.first_child()) ||
      (endParaRPr && endParaRPr.first_child()) || xfrmPr || wAttr || hAttr) {
    out << " style=\"";
    if (pPr)
      DefaultPropertyTransaltor(pPr, out, context);
    if (rPr)
      DefaultPropertyTransaltor(rPr, out, context);
    if (spPr)
      DefaultPropertyTransaltor(spPr, out, context);
    if (tcPr)
      TableCellPropertyTranslator(tcPr, out, context);
    if (endParaRPr)
      DefaultPropertyTransaltor(endParaRPr, out, context);
    if (xfrmPr)
      XfrmTranslator(xfrmPr, out, context);
    if (wAttr)
      out << "width:" << (wAttr.as_float() / 914400.0f) << "in;";
    if (hAttr)
      out << "height:" << (hAttr.as_float() / 914400.0f) << "in;";
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

void ParagraphTranslator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  out << "<p";
  ElementAttributeTranslator(in, out, context);
  out << ">";

  const auto buCharEle = in.child("a:pPr").child("a:buChar");
  if (buCharEle) {
    // const tinyxml2::XMLAttribute *charAttr =
    // buCharEle->FindAttribute("char"); const tinyxml2::XMLAttribute *sizeAttr
    // =
    // in.FirstChildElement("a:pPr")->FirstChildElement("a:buSzPct")->FindAttribute("val");
    // const tinyxml2::XMLAttribute *fontAttr =
    // in.FirstChildElement("a:pPr")->FirstChildElement("a:buFont")->FindAttribute("typeface");

    std::string bullet = "•";
    // TODO decide bullet, font, size on input

    out << "<span style=\"";
    // out << "font-family:" << fontAttr->Value() << " 2;font-size:" <<
    // sizeAttr->Int64Value() / 1440.0f << "pt;";
    out << "\">" << bullet << " </span>";
  }

  bool empty = true;
  for (auto &&e1 : in) {
    for (auto &&e2 : in) {
      if (common::StringUtil::endsWith(e1.name(), "Pr"))
        ;
      else if (std::strcmp(e1.name(), "w:r") != 0)
        empty = false;
      else if (common::StringUtil::endsWith(e2.name(), "Pr"))
        empty = false;
    }
  }

  if (empty)
    out << "<br/>";
  else
    ElementChildrenTranslator(in, out, context);

  out << "</p>";
}

void SpanTranslator(const pugi::xml_node &in, std::ostream &out,
                    Context &context) {
  bool link = false;
  const auto hlinkClick = in.child("a:rPr").child("a:hlinkClick");
  if (hlinkClick && hlinkClick.attribute("r:id")) {
    const auto rIdAttr = hlinkClick.attribute("r:id");
    const std::string href = context.relations[rIdAttr.as_string()];
    link = true;
    out << "<a href=\"" << href << "\">";
  }

  out << "<span";
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</span>";

  if (link)
    out << "</a>";
}

void SlideTranslator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << "<div class=\"slide\">";
  ElementChildrenTranslator(in, out, context);
  out << "</div>";
}

void TableTranslator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << R"(<table border="0" cellspacing="0" cellpadding="0")";
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";
}

// TODO duplicated in document translation
void ImageTranslator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << "<img";
  ElementAttributeTranslator(in, out, context);

  const auto ref = in.child("p:blipFill").child("a:blip");
  if (ref || ref.attribute("r:embed")) {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  } else {
    const auto rIdAttr = ref.attribute("r:embed");
    const auto path =
        access::Path("ppt/slides").join(context.relations[rIdAttr.as_string()]);
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
    if (n.text())
      TextTranslator(n.text(), out, context);
    else if (n)
      ElementTranslator(n, out, context);
  }
}

void ElementTranslator(const pugi::xml_node &in, std::ostream &out,
                       Context &context) {
  static std::unordered_map<std::string, const char *> substitution{
      {"p:sp", "div"},
      {"p:graphicFrame", "div"},
      {"a:tblGrid", "colgroup"},
      {"a:gridCol", "col"},
      {"a:tr", "tr"},
      {"a:tc", "td"},
  };
  static std::unordered_set<std::string> skippers{};

  const std::string element = in.name();
  if (skippers.find(element) != skippers.end())
    return;

  if (element == "a:p")
    ParagraphTranslator(in, out, context);
  else if (element == "a:r")
    SpanTranslator(in, out, context);
  else if (element == "p:cSld")
    SlideTranslator(in, out, context);
  else if (element == "a:tbl")
    TableTranslator(in, out, context);
  else if (element == "p:pic")
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

void PresentationTranslator::html(const pugi::xml_node &in, Context &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace ooxml
} // namespace odr
