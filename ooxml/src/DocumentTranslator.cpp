#include <Context.h>
#include <DocumentTranslator.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <access/StreamUtil.h>
#include <common/StringUtil.h>
#include <common/XmlUtil.h>
#include <crypto/CryptoUtil.h>
#include <glog/logging.h>
#include <odr/Config.h>
#include <string>
#include <tinyxml2.h>
#include <unordered_map>
#include <unordered_set>

namespace odr {
namespace ooxml {

namespace {
void AlignmentTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &) {
  const auto valAttr = in.FindAttribute("w:val");
  out << "text-align:" << valAttr->Value() << ";";
}

void FontTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                    Context &) {
  const auto fontAttr = in.FindAttribute("w:cs");
  if (fontAttr == nullptr)
    return;
  out << "font-family:" << fontAttr->Value() << ";";
}

void FontSizeTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &) {
  const auto sizeAttr = in.FindAttribute("w:val");
  if (sizeAttr == nullptr)
    return;
  const double size = std::stoi(sizeAttr->Value()) / 2.0;
  out << "font-size:" << size << "pt;";
}

void BoldTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                    Context &) {
  const auto valAttr = in.FindAttribute("w:val");
  if (valAttr != nullptr)
    return;
  out << "font-weight:bold;";
}

void ItalicTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                      Context &) {
  const auto valAttr = in.FindAttribute("w:val");
  if (valAttr != nullptr)
    return;
  out << "font-style:italic;";
}

void UnderlineTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &) {
  const auto valAttr = in.FindAttribute("w:val");
  if (std::strcmp(valAttr->Value(), "single") == 0)
    out << "text-decoration:underline;";
  // TODO wont work with StrikeThroughTranslator
}

void StrikeThroughTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                             Context &) {
  // TODO wont work with UnderlineTranslator

  const auto valAttr = in.FindAttribute("w:val");
  if ((valAttr != nullptr) && (std::strcmp(valAttr->Value(), "false") == 0))
    return;
  out << "text-decoration:line-through;";
}

void ShadowTranslator(const tinyxml2::XMLElement &, std::ostream &out,
                      Context &) {
  out << "text-shadow:1pt 1pt;";
}

void ColorTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &) {
  const auto valAttr = in.FindAttribute("w:val");
  if (std::strcmp(valAttr->Value(), "auto") == 0)
    return;
  if (std::strlen(valAttr->Value()) == 6)
    out << "color:#" << valAttr->Value();
  else
    out << "color:" << valAttr->Value();
}

void HighlightTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &) {
  const auto valAttr = in.FindAttribute("w:val");
  if (std::strcmp(valAttr->Value(), "auto") == 0)
    return;
  if (std::strlen(valAttr->Value()) == 6)
    out << "background-color:#" << valAttr->Value();
  else
    out << "background-color:" << valAttr->Value();
}

void IndentationTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                           Context &) {
  const tinyxml2::XMLAttribute *leftAttr = in.FindAttribute("w:left");
  if (leftAttr != nullptr)
    out << "margin-left:" << leftAttr->Int64Value() / 1440.0f << "in;";
  const tinyxml2::XMLAttribute *rightAttr = in.FindAttribute("w:right");
  if (rightAttr != nullptr)
    out << "margin-right:" << rightAttr->Int64Value() / 1440.0f << "in;";
}

void TableCellWidthTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                              Context &) {
  const tinyxml2::XMLAttribute *widthAttr = in.FindAttribute("w:w");
  const tinyxml2::XMLAttribute *typeAttr = in.FindAttribute("w:type");
  if (widthAttr != nullptr) {
    float width = widthAttr->Int64Value();
    if (typeAttr != nullptr && std::strcmp(typeAttr->Value(), "dxa") == 0)
      width /= 1440.0f;
    out << "width:" << width << "in;";
  }
}

void TableCellBorderTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out, Context &) {
  auto translator = [&](const char *name, const tinyxml2::XMLElement &e) {
    out << name << ":";

    const auto val = e.FindAttribute("w:val")->Value();
    if (std::strcmp(val, "nil") == 0) {
      out << "none";
    } else {
      const float sizePt = e.FindAttribute("w:sz")->Int64Value() / 2.0f;
      out << sizePt << "pt ";

      out << "solid ";

      const auto colorAttr = e.FindAttribute("w:color");
      if (std::strlen(colorAttr->Value()) == 6)
        out << "#" << colorAttr->Value();
      else
        out << colorAttr->Value();
    }

    out << ";";
  };

  const tinyxml2::XMLElement *top = in.FirstChildElement("w:top");
  if (top != nullptr)
    translator("border-top", *top);

  const tinyxml2::XMLElement *left = in.FirstChildElement("w:left");
  if (left != nullptr)
    translator("border-left", *left);

  const tinyxml2::XMLElement *bottom = in.FirstChildElement("w:bottom");
  if (bottom != nullptr)
    translator("border-bottom", *bottom);

  const tinyxml2::XMLElement *right = in.FirstChildElement("w:right");
  if (right != nullptr)
    translator("border-right", *right);
}

void translateStyleInline(const tinyxml2::XMLElement &in, std::ostream &out,
                          Context &context) {
  common::XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
    const std::string element = e.Name();

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
  });
}

void StyleClassTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                          Context &context) {
  const auto nameAttr = in.FindAttribute("w:styleId");
  std::string name = "unknown";
  if (nameAttr != nullptr) {
    name = nameAttr->Value();
  } else {
    LOG(WARNING) << "no name attribute " << in.Name();
  }

  const auto typeAttr = in.FindAttribute("w:type");
  std::string type = "unknown";
  if (typeAttr != nullptr) {
    type = typeAttr->Value();
  } else {
    LOG(WARNING) << "no type attribute " << in.Name();
  }

  /*
  const char *parentStyleName;
  if (in.QueryStringAttribute("style:parent-style-name", &parentStyleName) ==
  tinyxml2::XML_SUCCESS) {
      context.odStyleDependencies[styleName].push_back(parentStyleName);
  }
   */

  out << "." << name << " {";

  const auto pPr = in.FirstChildElement("w:pPr");
  if (pPr != nullptr)
    translateStyleInline(*pPr, out, context);

  const auto rPr = in.FirstChildElement("w:rPr");
  if (rPr != nullptr)
    translateStyleInline(*rPr, out, context);

  out << "}\n";
}
} // namespace

void DocumentTranslator::css(const tinyxml2::XMLElement &in, Context &context) {
  common::XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e) {
    const std::string element = e.Name();

    if (element == "w:style")
      StyleClassTranslator(e, *context.output, context);
    // else if (element == "w:docDefaults") DefaultStyleTranslator(e,
    // *context.output, context);
  });
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
                              Context &context) {
  const std::string prefix = in.Name();

  const tinyxml2::XMLElement *style =
      tinyxml2::XMLHandle((tinyxml2::XMLElement &)in)
          .FirstChildElement((prefix + "Pr").c_str())
          .FirstChildElement((prefix + "Style").c_str())
          .ToElement();
  if (style != nullptr) {
    *context.output << " class=\"" << style->FindAttribute("w:val")->Value()
                    << "\"";
  }

  const tinyxml2::XMLElement *inlineStyle =
      in.FirstChildElement((prefix + "Pr").c_str());
  if (inlineStyle != nullptr && inlineStyle->FirstChild() != nullptr) {
    out << " style=\"";
    translateStyleInline(*inlineStyle, out, context);
    out << "\"";
  }
}

void ElementAttributeTranslator(const tinyxml2::XMLElement &in,
                                std::ostream &out, Context &context) {
  StyleAttributeTranslator(in, out, context);
}

void ElementChildrenTranslator(const tinyxml2::XMLElement &in,
                               std::ostream &out, Context &context);
void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                       Context &context);

void TabTranslator(const tinyxml2::XMLElement &, std::ostream &out, Context &) {
  out << "\t";
}

void ParagraphTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &context) {
  const tinyxml2::XMLElement *num =
      tinyxml2::XMLHandle((tinyxml2::XMLElement &)in)
          .FirstChildElement("w:pPr")
          .FirstChildElement("w:numPr")
          .ToElement();
  bool listing = num != nullptr;
  int listingLevel;
  if (listing) {
    listingLevel =
        num->FirstChildElement("w:ilvl")->FindAttribute("w:val")->IntValue();
    for (int i = 0; i <= listingLevel; ++i)
      out << "<ul>";
    out << "<li>";
  }

  out << "<p";
  ElementAttributeTranslator(in, out, context);
  out << ">";

  bool empty = true;
  common::XmlUtil::visitElementChildren(
      in, [&](const tinyxml2::XMLElement &e1) {
        common::XmlUtil::visitElementChildren(
            e1, [&](const tinyxml2::XMLElement &e2) {
              if (common::StringUtil::endsWith(e1.Name(), "Pr"))
                ;
              else if (std::strcmp(e1.Name(), "w:r") != 0)
                empty = false;
              else if (!common::StringUtil::endsWith(e2.Name(), "Pr"))
                empty = false;
            });
      });

  if (empty)
    out << "<br/>";
  else
    ElementChildrenTranslator(in, out, context);

  out << "</p>";

  if (listing) {
    out << "</li>";
    for (int i = 0; i <= listingLevel; ++i)
      out << "</ul>";
  }
}

void SpanTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                    Context &context) {
  out << "<span";
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</span>";
}

void HyperlinkTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                         Context &context) {
  out << "<a";

  const tinyxml2::XMLAttribute *anchorAttr = in.FindAttribute("w:anchor");
  const tinyxml2::XMLAttribute *rIdAttr = in.FindAttribute("r:id");
  if (anchorAttr != nullptr)
    out << " href=\"#" << anchorAttr->Value() << "\" target=\"_self\"";
  else if (rIdAttr != nullptr)
    out << " href=\"" << context.relations[rIdAttr->Value()] << "\"";

  ElementAttributeTranslator(in, out, context);

  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</a>";
}

void BookmarkTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &) {
  const tinyxml2::XMLAttribute *nameAttr = in.FindAttribute("w:name");
  if (nameAttr != nullptr)
    out << "<a id=\"" << nameAttr->Value() << "\"/>";
}

void TableTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &context) {
  out << R"(<table border="0" cellspacing="0" cellpadding="0")";
  ElementAttributeTranslator(in, out, context);
  out << ">";
  ElementChildrenTranslator(in, out, context);
  out << "</table>";
}

void DrawingsTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                        Context &context) {
  // ooxml is using amazing units
  // https://startbigthinksmall.wordpress.com/2010/01/04/points-inches-and-emus-measuring-units-in-office-open-xml/

  const tinyxml2::XMLElement *child = common::XmlUtil::firstChildElement(in);
  if (child == nullptr)
    return;
  const tinyxml2::XMLElement *graphic = child->FirstChildElement("a:graphic");
  if (graphic == nullptr)
    return;
  // TODO handle something other than inline

  out << "<div";

  const tinyxml2::XMLElement *sizeEle = child->FirstChildElement("wp:extent");
  if (sizeEle != nullptr) {
    float widthIn = sizeEle->FindAttribute("cx")->Int64Value() / 914400.0f;
    float heightIn = sizeEle->FindAttribute("cy")->Int64Value() / 914400.0f;
    out << " style=\"width:" << widthIn << "in;height:" << heightIn << "in;\"";
  }

  out << ">";
  ElementTranslator(*graphic, out, context);
  out << "</div>";
}

void ImageTranslator(const tinyxml2::XMLElement &in, std::ostream &out,
                     Context &context) {
  out << "<img style=\"width:100%;height:100%\"";

  const tinyxml2::XMLElement *ref =
      tinyxml2::XMLHandle((tinyxml2::XMLElement &)in)
          .FirstChildElement("pic:blipFill")
          .FirstChildElement("a:blip")
          .ToElement();
  if (ref == nullptr || ref->FindAttribute("r:embed") == nullptr) {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  } else {
    const char *rIdAttr = ref->FindAttribute("r:embed")->Value();
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
      {"w:tr", "tr"},
      {"w:tc", "td"},
  };
  static std::unordered_set<std::string> skippers{
      "w:instrText",
  };

  const std::string element = in.Name();
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

void DocumentTranslator::html(const tinyxml2::XMLElement &in,
                              Context &context) {
  ElementTranslator(in, *context.output, context);
}

} // namespace ooxml
} // namespace odr
