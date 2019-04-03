#include "DocumentTranslator.h"
#include <fstream>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentFile.h"
#include "StyleTranslator.h"
#include "ContentTranslator.h"
#include "Context.h"

namespace odr {

class DefaultDocumentTranslatorImpl : public DocumentTranslator {
public:
    std::unique_ptr<StyleTranslator> styleTranslator;
    std::unique_ptr<ContentTranslator> contentTranslator;

    ~DefaultDocumentTranslatorImpl() override = default;

    bool translate(OpenDocumentFile &in, const std::string &out, Context &context) const override {
        if (!in.isFile("content.xml")) {
            return false;
        }

        std::ofstream of(out);
        if (!of.is_open()) {
            return false;
        }

        of << "<!DOCTYPE html>\n"
              "<html>\n"
              "<head>\n"
              "<meta charset=\"UTF-8\" />\n"
              "<base target=\"_blank\" />\n"
              "<meta name=\"viewport\" content=\"width=device-width; initial-scale=1.0; user-scalable=yes\" />\n"
              "<title>odr</title>\n";

        of << "<style>\n";
        generateStyle(in, of, context);
        auto contentXml = in.loadXML("content.xml");
        tinyxml2::XMLHandle contentHandle(contentXml.get());
        generateContentStyle(contentHandle, of, context);
        of << "</style>\n";

        of << "<script>\n";
        generateScript(of, context);
        of << "</script>\n";

        of << "</head>\n";
        of << "<body>\n";

        generateContent(in, contentHandle, of, context);

        of << "\n";
        of << "</body>\n";
        of << "</html>\n";

        of.close();
        return true;
    }

    void generateStyle(OpenDocumentFile &in, std::ofstream &of, Context &context) const {
        // TODO: get styles from translators?

        // default css
        of << "* {\n"
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

        auto stylesXml = in.loadXML("styles.xml");
        tinyxml2::XMLHandle stylesHandle(stylesXml.get());

        tinyxml2::XMLElement *fontFaceDecls = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:font-face-decls")
                .ToElement();
        styleTranslator->translate(*fontFaceDecls, of, context);

        tinyxml2::XMLElement *styles = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:styles")
                .ToElement();
        styleTranslator->translate(*styles, of, context);
    }
    void generateContentStyle(tinyxml2::XMLHandle &in, std::ofstream &of, Context &context) const {
        tinyxml2::XMLElement *fontFaceDecls = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:font-face-decls")
                .ToElement();
        styleTranslator->translate(*fontFaceDecls, of, context);

        tinyxml2::XMLElement *automaticStyles = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:automatic-styles")
                .ToElement();
        styleTranslator->translate(*automaticStyles, of, context);
    }
    void generateScript(std::ofstream &of, Context &context) const {
        // TODO: get script from translators?
    }
    void generateContent(OpenDocumentFile &file, tinyxml2::XMLHandle &in, std::ofstream &of, Context &context) const {
        tinyxml2::XMLHandle bodyHandle = in
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:body");
        tinyxml2::XMLElement *body = bodyHandle.ToElement();

        if ((context.config->entryOffset > 0) | (context.config->entryCount > 0)) {
            tinyxml2::XMLElement *content = nullptr;
            const char *entryName = nullptr;

            switch (file.getMeta().type) {
                case OpenDocumentFile::Meta::Type::SPREADSHEET:
                    content = bodyHandle.FirstChildElement("office:spreadsheet").ToElement();
                    entryName = "table:table";
                    break;
                case OpenDocumentFile::Meta::Type::PRESENTATION:
                    content = bodyHandle.FirstChildElement("office:presentation").ToElement();
                    entryName = "draw:page";
                    break;
                case OpenDocumentFile::Meta::Type::TEXT:
                case OpenDocumentFile::Meta::Type::GRAPHICS:
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

        contentTranslator->translate(*body, of, context);
    }
};

std::unique_ptr<DocumentTranslator> DocumentTranslator::create() {
    auto result = std::make_unique<DefaultDocumentTranslatorImpl>();
    result->styleTranslator = StyleTranslator::create();
    result->contentTranslator = ContentTranslator::create();
    return result;
}

}
