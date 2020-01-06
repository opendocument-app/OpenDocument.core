#include "OpenDocumentTranslator.h"
#include <fstream>
#include "tinyxml2.h"
#include "nlohmann/json.hpp"
#include "../Constants.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../io/Path.h"
#include "../io/ZipStorage.h"
#include "OpenDocumentFile.h"
#include "OpenDocumentStyleTranslator.h"
#include "OpenDocumentContentTranslator.h"

namespace odr {

class OpenDocumentTranslator::Impl final {
public:
    OpenDocumentStyleTranslator styleTranslator;
    OpenDocumentContentTranslator contentTranslator;

    bool translate(OpenDocumentFile &in, const std::string &outPath, TranslationContext &context) const {
        std::ofstream of(outPath);
        if (!of.is_open()) return false;
        context.output = &of;

        of << Constants::getHtmlBeginToStyle();

        generateStyle(of, context);
        context.content = in.loadXml("content.xml");
        tinyxml2::XMLHandle contentHandle(context.content.get());
        generateContentStyle(contentHandle, context);

        of << Constants::getHtmlStyleToBody();

        generateContent(in, contentHandle, context);

        of << Constants::getHtmlBodyToScript();

        generateScript(of, context);

        of << Constants::getHtmlScriptToEnd();

        of.close();
        return true;
    }

    void generateStyle(std::ofstream &out, TranslationContext &context) const {
        // TODO: get styles from translators?

        // default css
        out << Constants::getOpenDocumentDefaultCss();

        if (context.odFile->getMeta().type == FileType::OPENDOCUMENT_SPREADSHEET) {
            out << Constants::getOpenDocumentSpreadsheetDefaultCss();
        }

        auto stylesXml = context.odFile->loadXml("styles.xml");
        tinyxml2::XMLHandle stylesHandle(stylesXml.get());

        tinyxml2::XMLElement *fontFaceDecls = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:font-face-decls")
                .ToElement();
        if (fontFaceDecls != nullptr) {
            styleTranslator.translate(*fontFaceDecls, context);
        }

        tinyxml2::XMLElement *styles = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:styles")
                .ToElement();
        if (styles != nullptr) {
            styleTranslator.translate(*styles, context);
        }

        tinyxml2::XMLElement *automaticStyles = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:automatic-styles")
                .ToElement();
        if (automaticStyles != nullptr) {
            styleTranslator.translate(*automaticStyles, context);
        }
    }

    void generateContentStyle(tinyxml2::XMLHandle &in, TranslationContext &context) const {
        tinyxml2::XMLElement *fontFaceDecls = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:font-face-decls")
                .ToElement();
        if (fontFaceDecls != nullptr) {
            styleTranslator.translate(*fontFaceDecls, context);
        }

        tinyxml2::XMLElement *automaticStyles = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:automatic-styles")
                .ToElement();
        if (automaticStyles != nullptr) {
            styleTranslator.translate(*automaticStyles, context);
        }
    }

    void generateScript(std::ofstream &of, TranslationContext &context) const {
        of << Constants::getDefaultScript();
    }

    void generateContent(OpenDocumentFile &file, tinyxml2::XMLHandle &in, TranslationContext &context) const {
        tinyxml2::XMLHandle bodyHandle = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:body");
        tinyxml2::XMLElement *body = bodyHandle.ToElement();

        // TODO breaks back translation
        if ((context.config->entryOffset > 0) | (context.config->entryCount > 0)) {
            tinyxml2::XMLElement *content = nullptr;
            const char *entryName = nullptr;

            switch (file.getMeta().type) {
                case FileType::OPENDOCUMENT_TEXT:
                    break;
                case FileType::OPENDOCUMENT_PRESENTATION:
                    content = bodyHandle.FirstChildElement("office:presentation").ToElement();
                    entryName = "draw:page";
                    break;
                case FileType::OPENDOCUMENT_SPREADSHEET:
                    content = bodyHandle.FirstChildElement("office:spreadsheet").ToElement();
                    entryName = "table:table";
                    break;
                case FileType::OPENDOCUMENT_GRAPHICS:
                    break;
                default:
                    break;
            }

            if (content != nullptr) {
                std::uint32_t i = 0;
                tinyxml2::XMLElement *e = content->FirstChildElement(entryName);
                while (e != nullptr) {
                    tinyxml2::XMLElement *next = e->NextSiblingElement(entryName);
                    if ((i < context.config->entryOffset) ||
                            ((context.config->entryCount == 0) ||
                             (i >= context.config->entryOffset + context.config->entryCount))) {
                        content->DeleteChild(e);
                    }
                    ++i;
                    e = next;
                }
            }
        }

        contentTranslator.translate(*body, context);
    }

    bool backTranslate(OpenDocumentFile &in, const std::string &diff, const std::string &out, TranslationContext &context) const {
        // TODO exit on encrypted files

        const auto json = nlohmann::json::parse(diff);

        if (json.contains("modifiedText")) {
            for (auto &&i : json["modifiedText"].items()) {
                const auto it = context.textTranslation.find(std::stoi(i.key()));
                // TODO dirty const off-cast
                if (it == context.textTranslation.end()) continue;
                ((tinyxml2::XMLText *) it->second)->SetValue(i.value().get<std::string>().c_str());
            }
        }

        ZipWriter writer(out);
        in.getZipReader().visit([&](const auto &p) {
            if (p == "content.xml") return;
            writer.copy(in.getZipReader(), p);
        });

        tinyxml2::XMLPrinter printer(0, true, 0);
        context.content->Print(&printer);
        writer.write("content.xml")->write(printer.CStr(), printer.CStrSize() - 1);

        return true;
    }
};

OpenDocumentTranslator::OpenDocumentTranslator() :
        impl(std::make_unique<Impl>()){
}

OpenDocumentTranslator::~OpenDocumentTranslator() = default;

bool OpenDocumentTranslator::translate(OpenDocumentFile &in, const std::string &outPath, TranslationContext &context) const {
    return impl->translate(in, outPath, context);
}

bool OpenDocumentTranslator::backTranslate(OpenDocumentFile &in, const std::string &diff, const std::string &outPath, TranslationContext &context) const {
    return impl->backTranslate(in, diff, outPath, context);
}

}
