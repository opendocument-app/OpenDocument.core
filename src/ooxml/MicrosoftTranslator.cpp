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
#include "../XmlUtil.h"
#include "OfficeOpenXmlMeta.h"
#include "MicrosoftContentTranslator.h"
#include "MicrosoftStyleTranslator.h"

namespace odr {

class MicrosoftTranslator::Impl {
public:
    bool translate(const std::string &outPath, TranslationContext &context) const {
        std::ofstream of(outPath);
        if (!of.is_open()) return false;
        context.output = &of;

        of << Constants::getHtmlBeginToStyle();

        generateStyle(context);

        of << Constants::getHtmlStyleToBody();

        generateContent(context);

        of << Constants::getHtmlBodyToScript();

        generateScript(of, context);

        of << Constants::getHtmlScriptToEnd();

        of.close();
        return true;
    }

    void generateStyle(TranslationContext &context) const {
        switch (context.meta->type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT: {
                const auto stylesXml = XmlUtil::parse(*context.storage, "word/styles.xml");
                const tinyxml2::XMLElement *styles = stylesXml->RootElement();
                MicrosoftStyleTranslator::translate(*styles, context);
            } break;
            case FileType::OFFICE_OPEN_XML_PRESENTATION:
            case FileType::OFFICE_OPEN_XML_WORKBOOK:
                break;
            default:
                throw std::invalid_argument("file.getMeta().type");
        }
    }

    void generateScript(std::ofstream &of, TranslationContext &) const {
        of << Constants::getDefaultScript();
    }

    void generateContent(TranslationContext &context) const {
        switch (context.meta->type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT: {
                context.content = XmlUtil::parse(*context.storage, "word/document.xml");

                tinyxml2::XMLElement *body = context.content
                        ->FirstChildElement("w:document")
                        ->FirstChildElement("w:body");

                MicrosoftContentTranslator::translate(*body, context);
            } break;
            case FileType::OFFICE_OPEN_XML_PRESENTATION: {
                context.content = XmlUtil::parse(*context.storage, "ppt/presentation.xml");

                const auto rels = OfficeOpenXmlMeta::parseRelationships(*context.storage, "ppt/presentation.xml");
                XmlUtil::recursiveVisitElementsWithName(context.content->RootElement(), "p:sldId", [&](const auto &child) {
                    const std::string rId = child.FindAttribute("r:id")->Value();
                    const auto sheet = XmlUtil::parse(*context.storage, Path("ppt").join(rels.at(rId).target));
                    MicrosoftContentTranslator::translate(*sheet->RootElement(), context);
                });
            } break;
            case FileType::OFFICE_OPEN_XML_WORKBOOK: {
                context.content = XmlUtil::parse(*context.storage, "xl/sharedStrings.xml");

                // TODO this breaks back translation
                context.msSharedStringsDocument = XmlUtil::parse(*context.storage, "xl/sharedStrings.xml");
                XmlUtil::recursiveVisitElementsWithName(context.msSharedStringsDocument->RootElement(), "si", [&](const auto &child) {
                    context.msSharedStrings.push_back(&child);
                });

                const auto rels = OfficeOpenXmlMeta::parseRelationships(*context.storage, "xl/workbook.xml");
                XmlUtil::recursiveVisitElementsWithName(context.content->RootElement(), "sheet", [&](const auto &child) {
                    const std::string rId = child.FindAttribute("r:id")->Value();
                    const auto sheet = XmlUtil::parse(*context.storage, Path("xl").join(rels.at(rId).target));
                    MicrosoftContentTranslator::translate(*sheet->RootElement(), context);
                });
            } break;
            default:
                throw std::invalid_argument("file.getMeta().type");
        }
    }

    bool backTranslate(const std::string &diff, const std::string &outPath, TranslationContext &context) const {
        return false;
    }
};

MicrosoftTranslator::MicrosoftTranslator() :
        impl(std::make_unique<Impl>()) {
}

MicrosoftTranslator::~MicrosoftTranslator() = default;

bool MicrosoftTranslator::translate(const std::string &outPath, TranslationContext &context) const {
    return impl->translate(outPath, context);
}

bool MicrosoftTranslator::backTranslate(const std::string &diff, const std::string &outPath, TranslationContext &context) const {
    return impl->backTranslate(diff, outPath, context);
}

}
