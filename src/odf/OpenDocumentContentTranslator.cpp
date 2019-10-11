#include "OpenDocumentContentTranslator.h"
#include <string>
#include <list>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/FileMeta.h"
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
        const auto count = in.Int64Attribute("text:c", -1);
        if (count <= 0) {
            return;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            // TODO: use "&nbsp;"?
            *context.output << " ";
        }
    }
};

class TabTranslator final : public DefaultXmlTranslator {
public:
    void translate(const tinyxml2::XMLElement &, TranslationContext &context) const final {
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

    using DefaultXmlTranslator::translate;

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        context.currentTableRowStart = context.config->tableOffsetRows;
        context.currentTableRowEnd = context.currentTableRowStart + context.config->tableLimitRows;
        context.currentTableColStart = context.config->tableOffsetCols;
        context.currentTableColEnd = context.currentTableColStart + context.config->tableLimitCols;
        // TODO remove dirty file check; add simple table translator for odt/odp
        if ((context.meta->type == FileType::OPENDOCUMENT_SPREADSHEET) && context.config->tableLimitByDimensions) {
            context.currentTableRowEnd = std::min(context.currentTableRowEnd, context.currentTableRowStart + context.meta->entries[context.currentEntry].rowCount);
            context.currentTableColEnd = std::min(context.currentTableColEnd, context.currentTableColStart + context.meta->entries[context.currentEntry].columnCount);
        }
        context.currentTableLocation = {};
        context.odDefaultCellStyles.clear();

        DefaultElementTranslator::translate(in, context);

        ++context.currentEntry;
    }
};

class TableColumnTranslator final : public DefaultElementTranslator {
public:
    TableColumnTranslator() : DefaultElementTranslator("col") {}

    using DefaultXmlTranslator::translate;

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto repeatedAttribute = in.FindAttribute("table:number-columns-repeated");
        const auto repeated = repeatedAttribute == nullptr ? 1 : repeatedAttribute->UnsignedValue();
        const auto defaultCellStyleAttribute = in.FindAttribute("table:default-cell-style-name");
        // TODO we could use span instead
        for (std::uint32_t i = 0; i < repeated; ++i) {
            if (context.currentTableLocation.getNextCol() >= context.currentTableColEnd) {
                break;
            }
            if (context.currentTableLocation.getNextCol() >= context.currentTableColStart) {
                if (defaultCellStyleAttribute != nullptr) {
                    context.odDefaultCellStyles[context.currentTableLocation.getNextCol()] = defaultCellStyleAttribute->Value();
                }
                DefaultElementTranslator::translate(in, context);
            }
            context.currentTableLocation.addCol(1);
        }
    }
};

class TableRowTranslator final : public DefaultElementTranslator {
public:
    TableRowTranslator() : DefaultElementTranslator("tr") {}

    using DefaultXmlTranslator::translate;

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto repeatedAttribute = in.FindAttribute("table:number-rows-repeated");
        const auto repeated = repeatedAttribute == nullptr ? 1 : repeatedAttribute->UnsignedValue();
        context.currentTableLocation.addRow(0); // TODO hacky
        for (std::uint32_t i = 0; i < repeated; ++i) {
            if (context.currentTableLocation.getNextRow() >= context.currentTableRowEnd) {
                break;
            }
            if (context.currentTableLocation.getNextRow() >= context.currentTableRowStart) {
                DefaultElementTranslator::translate(in, context);
            }
            context.currentTableLocation.addRow(1);
        }
    }
};

class TableCellTranslator final : public DefaultElementTranslator {
public:
    tinyxml2::XMLDocument tmpDoc;
    tinyxml2::XMLElement *tmpEle;

    TableCellTranslator() : DefaultElementTranslator("td") {
        addAttributeTranslator("table:number-columns-spanned", "colspan");
        addAttributeTranslator("table:number-rows-spanned", "rowspan");
        // TODO limit span

        tmpEle = tmpDoc.NewElement("tmp");
    }

    using DefaultXmlTranslator::translate;

    void translate(const tinyxml2::XMLElement &in, TranslationContext &context) const final {
        const auto repeatedAttribute = in.FindAttribute("table:number-columns-repeated");
        const auto colspanAttribute = in.FindAttribute("table:number-columns-spanned");
        const auto rowspanAttribute = in.FindAttribute("table:number-rows-spanned");
        const auto repeated = repeatedAttribute == nullptr ? 1 : repeatedAttribute->UnsignedValue();
        const auto colspan = colspanAttribute == nullptr ? 1 : colspanAttribute->UnsignedValue();
        const auto rowspan = rowspanAttribute == nullptr ? 1 : rowspanAttribute->UnsignedValue();
        for (std::uint32_t i = 0; i < repeated; ++i) {
            if (context.currentTableLocation.getNextCol() >= context.currentTableColEnd) {
                break;
            }
            if (context.currentTableLocation.getNextCol() >= context.currentTableColStart) {
                DefaultElementTranslator::translate(in, context);
            }
            context.currentTableLocation.addCell(colspan, rowspan, 1);
        }
    }

    void translateElementAttributes(const tinyxml2::XMLElement &in, TranslationContext &context) const override {
        if (in.FindAttribute("table:style-name") == nullptr) {
            // TODO looks quite dirty; better options?
            const auto it = context.odDefaultCellStyles.find(context.currentTableLocation.getNextCol());
            if (it != context.odDefaultCellStyles.end()) {
                tmpEle->SetAttribute("table:style-name", it->second.c_str());
                const auto tmpAttr = tmpEle->FindAttribute("table:style-name");
                translate(*tmpAttr, context);
            }
        }

        DefaultElementTranslator::translateElementAttributes(in, context);
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
    void translate(const tinyxml2::XMLAttribute &in, TranslationContext &context) const final {
        const std::string styleName = OpenDocumentStyleTranslator::escapeStyleName(in.Value());

        *context.output << " class=\"";
        *context.output << styleName;

        { // handle style dependencies
            const auto it = context.odStyleDependencies.find(styleName);
            if (it == context.odStyleDependencies.end()) {
                // TODO remove ?
                LOG(WARNING) << "unknown style: " << styleName;
            } else {
                for (auto i = it->second.rbegin(); i != it->second.rend(); ++i) {
                    *context.output<< " " << *i;
                }
            }
        }

        /*
        { // handle column default styles
            const auto it = context.odDefaultCellStyles.find(context.currentTableCol);
            if (it != context.odDefaultCellStyles.end()) {
                *context.output << " " << it->second;
            }
        }
         */

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
