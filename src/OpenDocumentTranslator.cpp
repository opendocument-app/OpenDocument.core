#include "OpenDocumentTranslator.h"
#include <fstream>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentFile.h"
#include "OpenDocumentStyleTranslator.h"
#include "OpenDocumentContentTranslator.h"
#include "OpenDocumentContext.h"

namespace odr {

namespace {

class DefaultDocumentTranslatorImpl : public OpenDocumentTranslator {
public:
    std::unique_ptr<OpenDocumentStyleTranslator> styleTranslator;
    std::unique_ptr<OpenDocumentContentTranslator> contentTranslator;

    ~DefaultDocumentTranslatorImpl() override = default;

    bool translate(OpenDocumentFile &in, const std::string &out, OpenDocumentContext &context) const override {
        if (!in.isFile("content.xml")) {
            return false;
        }

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
        auto contentXml = in.loadXML("content.xml");
        tinyxml2::XMLHandle contentHandle(contentXml.get());
        generateContentStyle(contentHandle, context);
        of << "</style>\n";

        of << "<script>\n";
        generateScript(of, context);
        of << "</script>\n";

        of << "</head>\n";
        of << "<body>\n";

        generateContent(in, contentHandle, context);

        of << "\n";
        of << "</body>\n";
        of << "</html>\n";

        of.close();
        return true;
    }

    void generateStyle(std::ofstream &out, OpenDocumentContext &context) const {
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
              "\tbackground-color: transparent !important;\n"
              "\tpadding: 0 !important;\n"
              "}\n"
              "\n"
              "span {\n"
              "\tmargin: 0 !important;\n"
              "}\n"
              "table {\n"
              "\tborder-collapse: collapse;\n"
              "\tdisplay: block;\n"
              "}\n"
              "\n"
              "td {\n"
              "\tvertical-align: top;\n"
              "}\n"
              "\n"
              "p {\n"
              "\tfont-family: \"Arial\";\n"
              "\tfont-size: 10pt;\n"
              "}\n";

        auto stylesXml = context.file->loadXML("styles.xml");
        context.styles = stylesXml.get();
        tinyxml2::XMLHandle stylesHandle(stylesXml.get());

        tinyxml2::XMLElement *fontFaceDecls = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:font-face-decls")
                .ToElement();
        if (fontFaceDecls != nullptr) {
            styleTranslator->translate(*fontFaceDecls, context);
        }

        tinyxml2::XMLElement *styles = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:styles")
                .ToElement();
        styleTranslator->translate(*styles, context);

        context.styles = nullptr;
    }
    void generateContentStyle(tinyxml2::XMLHandle &in, OpenDocumentContext &context) const {
        tinyxml2::XMLElement *fontFaceDecls = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:font-face-decls")
                .ToElement();
        if (fontFaceDecls != nullptr) {
            styleTranslator->translate(*fontFaceDecls, context);
        }

        tinyxml2::XMLElement *automaticStyles = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:automatic-styles")
                .ToElement();
        styleTranslator->translate(*automaticStyles, context);
    }
    void generateScript(std::ofstream &of, OpenDocumentContext &context) const {
        // TODO: get script from translators?
    }
    void generateContent(OpenDocumentFile &file, tinyxml2::XMLHandle &in, OpenDocumentContext &context) const {
        tinyxml2::XMLHandle bodyHandle = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:body");
        tinyxml2::XMLElement *body = bodyHandle.ToElement();

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

        contentTranslator->translate(*body, context);
    }
};

}

std::unique_ptr<OpenDocumentTranslator> OpenDocumentTranslator::create() {
    auto result = std::make_unique<DefaultDocumentTranslatorImpl>();
    result->styleTranslator = OpenDocumentStyleTranslator::create();
    result->contentTranslator = OpenDocumentContentTranslator::create();
    return result;
}

}
