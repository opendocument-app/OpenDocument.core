#include "OpenDocumentContentTranslator.h"
#include <string>
#include <list>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../io/Path.h"
#include "../svm/Svm2Svg.h"
#include "../xml/XmlTranslator.h"
#include "../xml/Xml2Html.h"
#include "../crypto/CryptoUtil.h"
#include "OpenDocumentFile.h"
#include "OpenDocumentStyleTranslator.h"

namespace odr {

namespace {
class SpaceTranslator final : public DefaultXmlTranslator {
public:
    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        int64_t count = in.Int64Attribute("text:c", -1);
        if (count <= 0) {
            return;
        }
        // TODO: use maxRepeat
        for (uint64_t i = 0; i < count; ++i) {
            // TODO: use "&nbsp;"?
            *context.output << " ";
        }
    }
};

class TabTranslator final : public DefaultXmlTranslator {
public:
    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        // TODO: use "&emsp;"?
        *context.output << "\t";
    }
};

class LinkTranslator final : public DefaultElementTranslator {
public:
    LinkTranslator() : DefaultElementTranslator("a") {}

    void translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto href = in.FindAttribute("xlink:href");
        if (href != nullptr) {
            *context.output << " href\"" << href->Value() << "\"";
            // NOTE: there is a trim in java
            if ((std::strlen(href->Value()) > 0) && (href->Value()[0] == '#')) {
                *context.output << " target\"_self\"";
            }
        } else {
            LOG(WARNING) << "empty link";
        }

        DefaultElementTranslator::translateElementAttributes(in, context);
    }
};

class BookmarkTranslator final : public DefaultElementTranslator {
public:
    BookmarkTranslator() : DefaultElementTranslator("a") {}

    void translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto id = in.FindAttribute("text:name");
        if (id != nullptr) {
            *context.output << " id=\"" << id->Value() << "\"";
        } else {
            LOG(WARNING) << "empty bookmark";
        }

        DefaultElementTranslator::translateElementAttributes(in, context);
    }
};

class TableTranslator final : public DefaultElementTranslator {
public:
    TableTranslator() : DefaultElementTranslator("table") {
        addAttributes("border", "0");
        addAttributes("cellspacing", "0");
        addAttributes("cellpadding", "0");
    }
};

class TableColumnTranslator final : public DefaultElementTranslator {
public:
    TableColumnTranslator() : DefaultElementTranslator("col") {
        // TODO table:number-columns-repeated
    }
};

class TableRowTranslator final : public DefaultElementTranslator {
public:
    TableRowTranslator() : DefaultElementTranslator("tr") {
        // TODO table:number-rows-repeated
    }
};

class TableCellTranslator final : public DefaultElementTranslator {
public:
    TableCellTranslator() : DefaultElementTranslator("td") {
        addAttributeTranslator("table:number-columns-spanned", "colspan");
        addAttributeTranslator("table:number-rows-spanned", "rowspan");
        // TODO table:number-columns-repeated
    }
};

class FrameTranslator final : public DefaultElementTranslator {
public:
    FrameTranslator() : DefaultElementTranslator("div") {}

    void translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto width = in.FindAttribute("svg:width");
        const auto height = in.FindAttribute("svg:height");

        *context.output << " style=\"";
        if (width != nullptr) {
            *context.output << "width:" << width->Value() << ";";
        }
        if (height != nullptr) {
            *context.output << "height:" << height->Value() << ";";
        }
        *context.output << "\"";

        DefaultElementTranslator::translateElementAttributes(in, context);
    }
};

class ImageTranslator final : public DefaultElementTranslator {
public:
    ImageTranslator() : DefaultElementTranslator("img") {}

    void translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto href = in.FindAttribute("xlink:href");

        *context.output << " style=\"width:100%;heigth:100%\"";

        if (href == nullptr) {
            *context.output << " alt=\"Error: image path not specified";
            LOG(ERROR) << "image href not found";
        } else {
            const std::string &path = href->Value();
            *context.output << " alt=\"Error: image not found or unsupported: " << path << "\"";
#ifdef ODR_CRYPTO
            if (context.odFile->isFile(path)) {
                std::string image = context.odFile->loadEntry(path);
                if ((path.find("ObjectReplacements", 0) != std::string::npos) ||
                    (path.find(".svm", 0) != std::string::npos)) {
                    std::istringstream svmIn(image);
                    std::ostringstream svgOut;
                    Svm2Svg::translate(svmIn, svgOut);
                    image = svgOut.str();
                    *context.output << " src=\"data:image/svg+xml;base64, ";
                } else {
                    // hacky image/jpg working according to tom
                    *context.output << " src=\"data:image/jpg;base64, ";
                }
                *context.output << CryptoUtil::base64Encode(image);
            } else {
                LOG(ERROR) << "image not found " << path;
            }
#endif
            *context.output << "\"";
        }

        DefaultElementTranslator::translateElementAttributes(in, context);
    }
};

class StyleAttributeTranslator final : public DefaultXmlTranslator {
public:
    std::list<std::string> newClasses;

    void translate(const tinyxml2::XMLAttribute &in, TranslationContext &context) const final {
        const std::string styleName = OpenDocumentStyleTranslator::escapeStyleName(in.Value());
        const auto it = context.styleDependencies.find(styleName);

        *context.output << " class=\"";
        if (it == context.styleDependencies.end()) {
            // TODO remove ?
            LOG(WARNING) << "unknown style: " << styleName;
        } else {
            for (auto i = it->second.rbegin(); i != it->second.rend(); ++i) {
                *context.output << *i << " ";
            }
        }
        *context.output << styleName;
        for (auto &&c : newClasses) {
            *context.output << " " << c;
        }
        *context.output << "\"";
    }
};

class IgnoreHandler final : public XmlTranslator {
public:
    void translate(const tinyxml2::XMLElement &, TranslationContext &) const final {}
    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const final {}
    void translate(const tinyxml2::XMLText &, TranslationContext &) const final {}
};

class DefaultHandler final : public DefaultXmlTranslator {
public:
    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        translateElementChildren(in, context);
    }

    void translate(const tinyxml2::XMLAttribute &, TranslationContext &) const final {}
};
}

class OpenDocumentContentTranslator::Impl final : public DefaultXmlTranslator {
public:
    DefaultElementTranslator paragraphTranslator;
    DefaultElementTranslator headlineTranslator;
    DefaultElementTranslator spanTranslator;
    LinkTranslator linkTranslator;
    SpaceTranslator spaceTranslator;
    TabTranslator tabTranslator;
    DefaultElementTranslator breakTranslator;
    DefaultElementTranslator listTranslator;
    DefaultElementTranslator listItemTranslator;
    BookmarkTranslator bookmarkTranslator;
    TableTranslator tableTranslator;
    TableColumnTranslator tableColumnTranslator;
    TableRowTranslator tableRowTranslator;
    TableCellTranslator tableCellTranslator;
    FrameTranslator frameTranslator;
    ImageTranslator imageTranslator;

    StyleAttributeTranslator styleAttributeTranslator;

    IgnoreHandler skipper;
    DefaultHandler defaultHandler;

    Impl() :
            paragraphTranslator("p"),
            headlineTranslator("h"),
            spanTranslator("span"),
            breakTranslator("br"),
            listTranslator("ul"),
            listItemTranslator("li") {
        addElementDelegation("text:p", paragraphTranslator.setDefaultDelegation(this));
        addElementDelegation("text:h", headlineTranslator.setDefaultDelegation(this));
        addElementDelegation("text:span", spanTranslator.setDefaultDelegation(this));
        addElementDelegation("text:a", linkTranslator.setDefaultDelegation(this));
        addElementDelegation("text:s", spaceTranslator.setDefaultDelegation(this));
        addElementDelegation("text:tab", tabTranslator.setDefaultDelegation(this));
        addElementDelegation("text:line-break", breakTranslator.setDefaultDelegation(this));
        addElementDelegation("text:list", listTranslator.setDefaultDelegation(this));
        addElementDelegation("text:list-item", listItemTranslator.setDefaultDelegation(this));
        addElementDelegation("text:bookmark", bookmarkTranslator.setDefaultDelegation(this));
        addElementDelegation("text:bookmark-start", bookmarkTranslator.setDefaultDelegation(this));
        addElementDelegation("table:table", tableTranslator.setDefaultDelegation(this));
        addElementDelegation("table:table-column", tableColumnTranslator.setDefaultDelegation(this));
        addElementDelegation("table:table-row", tableRowTranslator.setDefaultDelegation(this));
        addElementDelegation("table:table-cell", tableCellTranslator.setDefaultDelegation(this));
        addElementDelegation("draw:frame", frameTranslator.setDefaultDelegation(this));
        addElementDelegation("draw:image", imageTranslator.setDefaultDelegation(this));
        addElementDelegation("svg:desc", skipper);

        addAttributeDelegation("text:style-name", styleAttributeTranslator);
        addAttributeDelegation("table:style-name", styleAttributeTranslator);
        addAttributeDelegation("table:default-cell-style-name", styleAttributeTranslator);
        addAttributeDelegation("draw:style-name", styleAttributeTranslator);

        defaultHandler.setDefaultDelegation(this);
        setDefaultDelegation(&defaultHandler);
    }

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        translateElementChild(in, context);
    }

    void translate(const tinyxml2::XMLText &in, TranslationContext &context) const final {
        if (!context.config->editable) {
            *context.output << in.Value();
        } else {
            *context.output << "<span contenteditable=\"true\" data-odr-cid=\""
                    << context.currentTextTranslationIndex << "\">" << in.Value() << "</span>";
            context.textTranslation[context.currentTextTranslationIndex] = &in;
            ++context.currentTextTranslationIndex;
        }
    }
};

OpenDocumentContentTranslator::OpenDocumentContentTranslator() :
        impl(std::make_unique<Impl>()) {
}

OpenDocumentContentTranslator::~OpenDocumentContentTranslator() = default;

void OpenDocumentContentTranslator::translate(const tinyxml2::XMLElement &in, TranslationContext &context) const {
    impl->translate(in, context);
}

}
