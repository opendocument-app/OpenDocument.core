#include "ContentTranslator.h"
#include <string>
#include <unordered_map>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"

namespace odr {

struct Context {
    TranslationConfig config;
};

class ElementTranslator {
public:
    virtual ~ElementTranslator() = default;
    virtual void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const = 0;
    virtual void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const = 0;
};

class HierarchyTranslator {
public:
    virtual ~HierarchyTranslator() = default;
    virtual void translate(tinyxml2::XMLElement &in, std::ostream &out) const = 0;
};

class DefaultElementTranslator : public ElementTranslator {
public:
    std::string name;

    explicit DefaultElementTranslator(const std::string &name) : name(name) {}
    ~DefaultElementTranslator() override = default;
    void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const override {
        out << "<" << name << ">";
    }
    void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const override {
        out << "</" << name << ">";
    }
};

class SpaceTranslator : public ElementTranslator {
public:
    ~SpaceTranslator() override = default;
    void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const override {
        int64_t count = in.Int64Attribute("text:c", -1);
        if (count <= 0) {
            return;
        }
        // TODO: use maxRepeat
        for (uint64_t i = 0; i < count; ++i) {
            // TODO: use "&nbsp;"?
            out << " ";
        }
    }
    void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const override {}
};

class TabTranslator : public ElementTranslator {
public:
    ~TabTranslator() override = default;
    void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const override {
        // TODO: use "&emsp;"?
        out << "\t";
    }
    void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const override {}
};

class LinkTranslator : public ElementTranslator {
public:
    // https://github.com/andiwand/OpenDocument.java/blob/master/src/at/stefl/opendocument/java/translator/content/LinkTranslator.java
    ~LinkTranslator() override = default;
    void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const override {
        auto href = in.FindAttribute("xlink:href");
        out << "<a";
        if (href != nullptr) {
            out << " href\"" << href->Value() << "\"";
            // NOTE: there is a trim in java
            if ((std::strlen(href->Value()) > 0) && (href->Value()[0] == '#')) {
                out << " target\"_self\"";
            }
        } else {
            LOG(WARNING) << "empty link";
        }
        out << ">";
    }
    void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const override {
        out << "</a>";
    }
};

class BookmarkTranslator : public ElementTranslator {
public:
    // https://github.com/andiwand/OpenDocument.java/blob/development/src/at/stefl/opendocument/java/translator/content/BookmarkTranslator.java
    ~BookmarkTranslator() override = default;
    void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const override {
        auto id = in.FindAttribute("text:name");
        out << "<a";
        if (id != nullptr) {
            out << " id=\"" << id->Value() << "\"";
        } else {
            LOG(WARNING) << "empty bookmark";
        }
        out << ">";
    }
    void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const override {
        out << "</a>";
    }
};

class TableTranslator : public ElementTranslator {
public:
    ~TableTranslator() override = default;
    void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const override {
        out << "<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\">";
    }
    void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const override {
        out << "</table>";
    }
};

class TableCellTranslator : public ElementTranslator {
public:
    ~TableCellTranslator() override = default;
    void translateStart(tinyxml2::XMLElement &in, std::ostream &out) const override {
        int64_t colspan = in.Int64Attribute("table:number-columns-spanned", -1);
        int64_t rowspan = in.Int64Attribute("table:number-rows-spanned", -1);
        out << "<td";
        // TODO: use maxRepeat
        if (colspan > 1) {
            out << " colspan=\"" << colspan << "\"";
        }
        if (rowspan > 1) {
            out << " rowspan=\"" << rowspan << "\"";
        }
        out << ">";
    }
    void translateEnd(tinyxml2::XMLElement &in, std::ostream &out) const override {
        out << "</td>";
    }
};

class DefaultContentTranslatorImpl : public ContentTranslator {
public:
    std::unordered_map<std::string, std::unique_ptr<ElementTranslator>> elementTranslator;

    DefaultContentTranslatorImpl() {
        // https://github.com/andiwand/OpenDocument.java/blob/master/src/at/stefl/opendocument/java/translator/content/ParagraphTranslator.java
        elementTranslator["text:p"] = std::make_unique<DefaultElementTranslator>("p");
        elementTranslator["text:h"] = std::make_unique<DefaultElementTranslator>("h");
        // https://github.com/andiwand/OpenDocument.java/blob/master/src/at/stefl/opendocument/java/translator/content/SpanTranslator.java
        elementTranslator["text:span"] = std::make_unique<DefaultElementTranslator>("span");
        elementTranslator["text:s"] = std::make_unique<SpaceTranslator>();
        elementTranslator["text:tab"] = std::make_unique<TabTranslator>();
        elementTranslator["text:line-break"] = std::make_unique<DefaultElementTranslator>("br");
        elementTranslator["text:soft-page-break"] = nullptr;
        elementTranslator["text:list"] = std::make_unique<DefaultElementTranslator>("ul");
        elementTranslator["text:list-item"] = std::make_unique<DefaultElementTranslator>("li");
        elementTranslator["text:a"] = std::make_unique<LinkTranslator>();
        elementTranslator["text:bookmark"] = std::make_unique<BookmarkTranslator>();
        elementTranslator["text:bookmark-start"] = std::make_unique<BookmarkTranslator>();
        elementTranslator["text:bookmark-end"] = nullptr;
        elementTranslator["table:table"] = std::make_unique<TableTranslator>();
        elementTranslator["table:table-column"] = std::make_unique<DefaultElementTranslator>("col");
        elementTranslator["table:table-row"] = std::make_unique<DefaultElementTranslator>("tr");
        elementTranslator["table:table-cell"] = std::make_unique<TableCellTranslator>();
        elementTranslator["table:tracked-changes"] = nullptr;
    }
    ~DefaultContentTranslatorImpl() override = default;

    void translate(tinyxml2::XMLElement &in, std::ostream &out) const override {
        const std::string elementName = in.Name();
        auto elementTranslatorIt = elementTranslator.find(elementName);
        const ElementTranslator *translator = nullptr;
        if (elementTranslatorIt != elementTranslator.end()) {
            translator = elementTranslatorIt->second.get();
        } else {
            LOG(WARNING) << "unhandled element: " << elementName;
        }

        if (translator != nullptr) {
            translator->translateStart(in, out);
        }

        for (auto child = in.FirstChild(); child != nullptr; child = child->NextSibling()) {
            if (child->ToText() != nullptr) {
                out << child->ToText()->Value();
            } else if (child->ToElement() != nullptr) {
                translate(*child->ToElement(), out);
            }
        }

        if (translator != nullptr) {
            translator->translateEnd(in, out);
        }
    }
};

std::unique_ptr<ContentTranslator> ContentTranslator::create(const TranslationConfig &) {
    return std::make_unique<DefaultContentTranslatorImpl>();
}

}
