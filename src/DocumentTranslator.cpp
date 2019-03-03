#include "DocumentTranslator.h"
#include <fstream>
#include "OpenDocumentFile.h"
#include "StyleTranslator.h"
#include "ContentTranslator.h"

namespace opendocument {

bool DocumentTranslator::translate(OpenDocumentFile &in, const std::string &out) const {
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
        getStyleTranslator().translate(*fontFaceDecls, of);

        tinyxml2::XMLElement *styles = stylesHandle
                .FirstChildElement("office:document-styles")
                .FirstChildElement("office:styles")
                .ToElement();
        getStyleTranslator().translate(*styles, of);
    }

    {
        auto contentXml = in.loadXML("content.xml");
        tinyxml2::XMLHandle contentHandle(contentXml.get());

        tinyxml2::XMLElement *fontFaceDecls = contentHandle
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:font-face-decls")
                .ToElement();
        getStyleTranslator().translate(*fontFaceDecls, of);

        tinyxml2::XMLElement *automaticStyles = contentHandle
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:automatic-styles")
                .ToElement();
        getStyleTranslator().translate(*automaticStyles, of);

        of << "</style>\n"
              "</head>\n"
              "<body>\n";

        tinyxml2::XMLElement *body = contentHandle
                .FirstChildElement("office:document-content")
                .FirstChildElement("office:body")
                .ToElement();
        getContentTranslator().translate(*body, of);
    }

    of << "\n"
          "</body>\n"
          "</html>";

    of.close();
    return true;
}

const StyleTranslator& TextDocumentTranslator::getStyleTranslator() const {
    return styleTranslator_;
}

const ContentTranslator& TextDocumentTranslator::getContentTranslator() const {
    return contentTranslator_;
}

}
