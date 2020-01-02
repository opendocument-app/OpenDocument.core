#include "MicrosoftTranslator.h"
#include <fstream>
#include <stdexcept>
#include "tinyxml2.h"
#include "../Constants.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../io/Path.h"
#include "../io/ZipStorage.h"
#include "../xml/XmlUtil.h"
#include "MicrosoftOpenXmlFile.h"
#include "MicrosoftContentTranslator.h"

namespace odr {

namespace {
struct Relationship {
    // type
    Path target;
};
std::unordered_map<std::string, Relationship> parseRelationships(tinyxml2::XMLDocument &rels) {
    std::unordered_map<std::string, Relationship> result;
    XmlUtil::recursiveVisitElementsWithName(rels.RootElement(), "Relationship", [&](const auto &rel) {
        const std::string rId = rel.FindAttribute("Id")->Value();
        const Relationship r = {
                .target = rel.FindAttribute("Target")->Value()
        };
        result.insert({rId, r});
    });
    return result;
}
}

class MicrosoftTranslator::Impl {
public:
    MicrosoftContentTranslator contentTranslator;

    bool translate(MicrosoftOpenXmlFile &in, const std::string &outPath, TranslationContext &context) const {
        std::ofstream of(outPath);
        if (!of.is_open()) return false;
        context.output = &of;

        of << Constants::getHtmlBeginToStyle();

        generateStyle(of, context);

        of << Constants::getHtmlStyleToBody();

        context.content = in.loadContent();
        generateContent(in, *context.content, context);

        of << Constants::getHtmlBodyToScript();

        generateScript(of, context);

        of << Constants::getHtmlScriptToEnd();

        of.close();
        return true;
    }

    void generateStyle(std::ofstream &of, TranslationContext &) const {
    }

    void generateScript(std::ofstream &of, TranslationContext &) const {
        of << Constants::getDefaultScript();
    }

    void generateContent(MicrosoftOpenXmlFile &file, tinyxml2::XMLDocument &in, TranslationContext &context) const {
        switch (file.getMeta().type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT: {
                tinyxml2::XMLHandle bodyHandle = tinyxml2::XMLHandle(in)
                        .FirstChildElement("w:document")
                        .FirstChildElement("w:body");
                tinyxml2::XMLElement *body = in
                        .FirstChildElement("w:document")
                        ->FirstChildElement("w:body");

                contentTranslator.translate(*body, context);
            } break;
            case FileType::OFFICE_OPEN_XML_PRESENTATION: {
                const auto rels = parseRelationships(*file.loadRelationships(file.getContentPath()));
                XmlUtil::recursiveVisitElementsWithName(in.RootElement(), "p:sldId", [&](const auto &child) {
                    const std::string rId = child.FindAttribute("r:id")->Value();
                    const auto sheet = file.loadXml(Path("ppt").join(rels.at(rId).target));
                    contentTranslator.translate(*sheet->RootElement(), context);
                });
            } break;
            case FileType::OFFICE_OPEN_XML_WORKBOOK: {
                // TODO this breaks back translation
                context.msSharedStringsDocument = file.loadXml("xl/sharedStrings.xml");
                XmlUtil::recursiveVisitElementsWithName(context.msSharedStringsDocument->RootElement(), "si", [&](const auto &child) {
                    context.msSharedStrings.push_back(&child);
                });

                const auto rels = parseRelationships(*file.loadRelationships(file.getContentPath()));
                XmlUtil::recursiveVisitElementsWithName(in.RootElement(), "sheet", [&](const auto &child) {
                    const std::string rId = child.FindAttribute("r:id")->Value();
                    const auto sheet = file.loadXml(Path("xl").join(rels.at(rId).target));
                    contentTranslator.translate(*sheet->RootElement(), context);
                });
            } break;
            default:
                throw std::invalid_argument("file.getMeta().type");
        }
    }

    bool backTranslate(MicrosoftOpenXmlFile &in, const std::string &diff, const std::string &outPath, TranslationContext &context) const {
        return false;
    }
};

MicrosoftTranslator::MicrosoftTranslator() :
        impl(std::make_unique<Impl>()) {
}

MicrosoftTranslator::~MicrosoftTranslator() = default;

bool MicrosoftTranslator::translate(MicrosoftOpenXmlFile &in, const std::string &outPath, TranslationContext &context) const {
    return impl->translate(in, outPath, context);
}

bool MicrosoftTranslator::backTranslate(MicrosoftOpenXmlFile &in, const std::string &diff, const std::string &outPath, TranslationContext &context) const {
    return impl->backTranslate(in, diff, outPath, context);
}

}
