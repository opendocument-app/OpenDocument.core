#include "MicrosoftContentTranslator.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../StringUtil.h"
#include "../io/Storage.h"
#include "../io/StreamUtil.h"
#include "../crypto/CryptoUtil.h"
#include "../XmlUtil.h"
#include "MicrosoftStyleTranslator.h"

namespace odr {

static void TextTranslator(const tinyxml2::XMLText &in, std::ostream &out, TranslationContext &context);
static void AttributeTranslator(const tinyxml2::XMLAttribute &in, std::ostream &out, TranslationContext &context);
static void ElementAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);
static void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);
static void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context);

static void ParagraphTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<p";
    ElementAttributeTranslator(in, out, context);
    out << ">";

    // TODO inefficient
    bool empty = true;
    XmlUtil::visitElementChildren(in, [&](const tinyxml2::XMLElement &e1) {
        XmlUtil::visitElementChildren(e1, [&](const tinyxml2::XMLElement &e2) {
            if (std::strcmp(e1.Name(), "w:r") == 0 && std::strcmp(e2.Name(), "w:rPr") != 0) {
                empty = false;
            }
        });
    });

    if (empty) out << "<br/>";
    else ElementChildrenTranslator(in, out, context);

    out << "</p>";
}

static void SpanTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<span";
    ElementAttributeTranslator(in, out, context);
    out << ">";
    ElementChildrenTranslator(in, out, context);
    out << "</span>";
}

static void WordTableTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << R"(<table border="0" cellspacing="0" cellpadding="0")";
    ElementAttributeTranslator(in, out, context);
    out << ">";
    ElementChildrenTranslator(in, out, context);
    out << "</table>";
}

static void ImageTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << "<img";

    const tinyxml2::XMLElement *ref = tinyxml2::XMLHandle((tinyxml2::XMLElement &) in)
            .FirstChildElement("pic:blipFill")
            .FirstChildElement("a:blip")
            .ToElement();
    if (ref == nullptr || ref->FindAttribute("r:embed") == nullptr) {
        out << " alt=\"Error: image path not specified";
        LOG(ERROR) << "image href not found";
    } else {
        const char *rId = ref->FindAttribute("r:embed")->Value();
        const auto it = context.msRelations.find(rId);
        if (it != context.msRelations.end()) {
            const Path path = context.msRoot.join(it->second);
            out << " alt=\"Error: image not found or unsupported: " << path << "\"";
#ifdef ODR_CRYPTO
            out << " src=\"";
            std::string image = StreamUtil::read(*context.storage->read(path));
            // hacky image/jpg working according to tom
            out << "data:image/jpg;base64, ";
            out << CryptoUtil::base64Encode(image);
            out << "\"";
#endif
        }
    }

    out << ">";
}

static void TableTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    out << R"(<table border="0" cellspacing="0" cellpadding="0")";
    ElementAttributeTranslator(in, out, context);
    out << ">";
    ElementChildrenTranslator(in, out, context);
    out << "</table>";
}

static void TableCellTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    //const auto r = in.FindAttribute("r");
    // TODO check placement r and fill empty cells/rows

    out << "<td";
    ElementAttributeTranslator(in, out, context);
    out << ">";

    const auto t = in.FindAttribute("t");
    if (t != nullptr) {
        if (std::strcmp(t->Value(), "s") == 0) {
            const auto sharedStringIndex = in.FirstChildElement("v")->IntText(-1);
            if (sharedStringIndex >= 0) {
                const tinyxml2::XMLElement &replacement = *context.msSharedStrings[sharedStringIndex];
                ElementChildrenTranslator(replacement, out, context);
            } else {
                DLOG(INFO) << "undefined behaviour: shared string not found";
            }
        } else if ((std::strcmp(t->Value(), "inlineStr") == 0) ||
                   (std::strcmp(t->Value(), "n") == 0)) {
            ElementChildrenTranslator(in, out, context);
        } else {
            DLOG(INFO) << "undefined behaviour: t=" << t->Value();
        }
    } else {
        DLOG(INFO) << "undefined behaviour: t not found";
    }

    out << "</td>";
}

static void TextTranslator(const tinyxml2::XMLText &in, std::ostream &out, TranslationContext &context) {
    std::string text = in.Value();
    StringUtil::findAndReplaceAll(text, "&", "&amp;");
    StringUtil::findAndReplaceAll(text, "<", "&lt;");
    StringUtil::findAndReplaceAll(text, ">", "&gt;");

    if (!context.config->editable) {
        out << text;
    } else {
        out << R"(<span contenteditable="true" data-odr-cid=")"
            << context.currentTextTranslationIndex << "\">" << text << "</span>";
        context.textTranslation[context.currentTextTranslationIndex] = &in;
        ++context.currentTextTranslationIndex;
    }
}

static void StyleAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    const std::string prefix = in.Name();

    const tinyxml2::XMLElement *style = tinyxml2::XMLHandle((tinyxml2::XMLElement &) in)
            .FirstChildElement((prefix + "Pr").c_str())
            .FirstChildElement((prefix + "Style").c_str())
            .ToElement();
    if (style != nullptr) {
        *context.output << " class=\"" << style->FindAttribute("w:val")->Value() <<  "\"";
    }

    const tinyxml2::XMLElement *inlineStyle = in.FirstChildElement((prefix + "Pr").c_str());
    if (inlineStyle != nullptr && inlineStyle->FirstChild() != nullptr) {
        out << " style=\"";
        MicrosoftStyleTranslator::translateInline(*inlineStyle, out, context);
        out << "\"";
    }
}

static void AttributeTranslator(const tinyxml2::XMLAttribute &, std::ostream &, TranslationContext &) {
}

static void ElementAttributeTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    StyleAttributeTranslator(in, out, context);

    XmlUtil::visitElementAttributes(in, [&](const tinyxml2::XMLAttribute &a) {
        AttributeTranslator(a, out, context);
    });
}

static void ElementChildrenTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    XmlUtil::visitNodeChildren(in, [&](const tinyxml2::XMLNode &n) {
        if (n.ToText() != nullptr) TextTranslator(*n.ToText(), out, context);
        else if (n.ToElement() != nullptr) ElementTranslator(*n.ToElement(), out, context);
    });
}

static void ElementTranslator(const tinyxml2::XMLElement &in, std::ostream &out, TranslationContext &context) {
    static std::unordered_map<std::string, const char *> substitution{
            // document
            {"w:tr", "tr"},
            {"w:tc", "td"},
            // presentation
            {"p:cSld", "div"},
            {"p:sp", "div"},
            {"a:p", "p"},
            // workbook
            {"row", "tr"},
    };
    static std::unordered_set<std::string> skippers{
            // document
            "w:instrText",
            // workbook
            "headerFooter",
    };

    const std::string element = in.Name();
    if (skippers.find(element) != skippers.end()) return;

    if (element == "w:p") ParagraphTranslator(in, out, context);
    else if (element == "w:r") SpanTranslator(in, out, context);
    else if (element == "w:tbl") WordTableTranslator(in, out, context);
    else if (element == "pic:pic") ImageTranslator(in, out, context);
    else if (element == "worksheet") TableTranslator(in, out, context);
    else if (element == "c") TableCellTranslator(in, out, context);
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

void MicrosoftContentTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) {
    ElementTranslator(in, *context.output, context);
}

}
