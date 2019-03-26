#include "DocumentTranslator.h"
#include <fstream>
#include "tinyxml2.h"
#include "odr/TranslationConfig.h"
#include "OpenDocumentFile.h"
#include "StyleTranslator.h"
#include "ContentTranslator.h"

namespace odr {

class DefaultDocumentTranslatorImpl : public DocumentTranslator {
public:
    std::unique_ptr<StyleTranslator> styleTranslator;
    std::unique_ptr<ContentTranslator> contentTranslator;

    ~DefaultDocumentTranslatorImpl() override = default;

    bool translate(OpenDocumentFile &in, const std::string &out) const override {
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
              "<meta charset=\"UTF-8\">\n"
              "<title>odr</title>\n"
              "<style>\n";

        {
            auto stylesXml = in.loadXML("styles.xml");
            tinyxml2::XMLHandle stylesHandle(stylesXml.get());

            tinyxml2::XMLElement *fontFaceDecls = stylesHandle
                    .FirstChildElement("office:document-styles")
                    .FirstChildElement("office:font-face-decls")
                    .ToElement();
            styleTranslator->translate(*fontFaceDecls, of);

            tinyxml2::XMLElement *styles = stylesHandle
                    .FirstChildElement("office:document-styles")
                    .FirstChildElement("office:styles")
                    .ToElement();
            styleTranslator->translate(*styles, of);
        }

        {
            auto contentXml = in.loadXML("content.xml");
            tinyxml2::XMLHandle contentHandle(contentXml.get());

            tinyxml2::XMLElement *fontFaceDecls = contentHandle
                    .FirstChildElement("office:document-content")
                    .FirstChildElement("office:font-face-decls")
                    .ToElement();
            styleTranslator->translate(*fontFaceDecls, of);

            tinyxml2::XMLElement *automaticStyles = contentHandle
                    .FirstChildElement("office:document-content")
                    .FirstChildElement("office:automatic-styles")
                    .ToElement();
            styleTranslator->translate(*automaticStyles, of);

            of << "</style>\n"
                  "</head>\n"
                  "<body>\n";

            tinyxml2::XMLElement *body = contentHandle
                    .FirstChildElement("office:document-content")
                    .FirstChildElement("office:body")
                    .ToElement();
            contentTranslator->translate(*body, of);
        }

        of << "\n"
              "</body>\n"
              "</html>";

        of.close();
        return true;
    }
};

std::unique_ptr<DocumentTranslator> DocumentTranslator::create(const TranslationConfig &config) {
    auto result = std::make_unique<DefaultDocumentTranslatorImpl>();
    result->styleTranslator = StyleTranslator::create(config);
    result->contentTranslator = ContentTranslator::create(config);
    return result;
}

}
