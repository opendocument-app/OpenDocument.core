#include "OpenDocumentTranslator.h"
#include <fstream>
#include "tinyxml2.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../io/Path.h"
#include "../io/FileUtil.h"
#include "../io/ZipStorage.h"
#include "../xml/XmlUtil.h"
#include "OpenDocumentFile.h"
#include "OpenDocumentStyleTranslator.h"
#include "OpenDocumentContentTranslator.h"

namespace odr {

class OpenDocumentTranslator::Impl final {
public:
    OpenDocumentStyleTranslator styleTranslator;
    OpenDocumentContentTranslator contentTranslator;

    bool translate(OpenDocumentFile &in, const std::string &out, TranslationContext &context) const {
        std::ofstream of(out);
        if (!of.is_open()) {
            return false;
        }
        context.output = &of;

        of << "<!DOCTYPE html>\n"
              "<html>\n"
              "<head>\n"
              "<meta charset=\"UTF-8\" />\n"
              "<base target=\"_blank\" />\n"
              "<meta name=\"viewport\" content=\"width=device-width; initial-scale=1.0; user-scalable=yes\" />\n"
              "<title>odr</title>\n";

        of << "<style>\n";
        generateStyle(of, context);
        context.content = in.loadXml("content.xml");
        tinyxml2::XMLHandle contentHandle(context.content.get());
        generateContentStyle(contentHandle, context);
        of << "</style>\n";

        of << "<script>\n";
        generateScript(of, context);
        of << "</script>\n";

        of << "</head>\n";
        of << "<body>\n";

        generateContent(in, contentHandle, context);

        of << "</body>\n";
        of << "</html>";

        of.close();
        return true;
    }

    void generateStyle(std::ofstream &out, TranslationContext &context) const {
        // TODO: get styles from translators?

        // default css
        out << "* {\n"
              "\tmargin: 0px;\n"
              "\tposition: relative;\n"
              "}"
              "\tbody {\n"
              "\tpadding: 5px;\n"
              "}\n"
              "\tspan {\n"
              "\twhite-space: pre-wrap;\n"
              "}\n"
              "table {\n"
              "\ttable-layout: fixed;\n"
              "\twidth: 0px;\n"
              "}\n"
              "p {\n"
              "\tpadding: 0 !important;\n"
              "}\n"
              "\n"
              "span {\n"
              "\tmargin: 0 !important;\n"
              "}\n";

        if (context.odFile->getMeta().type == FileType::OPENDOCUMENT_SPREADSHEET) {
            out <<
                "table {\n"
                "\tborder-collapse: collapse;\n"
                "\tdisplay: block;\n"
                "}\n"
                "td {\n"
                "\tvertical-align: top;\n"
                "}\n"
                "p {\n"
                "\tfont-family: \"Arial\";\n"
                "\tfont-size: 10pt;\n"
                "}\n";
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
        styleTranslator.translate(*styles, context);
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
        styleTranslator.translate(*automaticStyles, context);
    }

    void generateScript(std::ofstream &of, TranslationContext &context) const {
        // TODO: get script from translators?
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
};

OpenDocumentTranslator::OpenDocumentTranslator() :
        impl(std::make_unique<Impl>()){
}

OpenDocumentTranslator::~OpenDocumentTranslator() = default;

bool OpenDocumentTranslator::translate(OpenDocumentFile &in, const std::string &out, TranslationContext &context) const {
    return impl->translate(in, out, context);
}

}
