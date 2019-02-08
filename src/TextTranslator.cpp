#include "TextTranslator.h"
#include <fstream>
#include "OpenDocumentFile.h"

namespace opendocument {

TextTranslator::TextTranslator() {
}

TextTranslator::~TextTranslator() {
}

void translateElement(tinyxml2::XMLElement *in, std::ofstream &out) {
    if (std::strcmp("text:p", in->Name()) == 0) {
        out << "<p>";
    }

    for (auto child = in->FirstChild(); child != nullptr; child = child->NextSibling()) {
        if (child->ToText() != nullptr) {
            out << child->ToText()->Value();
        } else if (child->ToElement() != nullptr) {
            translateElement(child->ToElement(), out);
        }
    }

    if (std::strcmp("text:p", in->Name()) == 0) {
        out << "</p>";
    }
}

bool TextTranslator::translate(OpenDocumentFile &in, const std::string &out) const {
    if (!in.isFile("content.xml")) {
        return false;
    }

    auto content = in.loadXML("content.xml");
    tinyxml2::XMLHandle contentHandle(content.get());
    tinyxml2::XMLElement *textBody = contentHandle
            .FirstChildElement("office:document-content")
            .FirstChildElement("office:body")
            .ToElement();

    std::ofstream of(out);
    if (!of.is_open()) {
        return false;
    }

    of << "<!DOCTYPE html>\n"
           "<html>\n"
           "<head>\n"
           "<meta charset=\"UTF-8\">\n"
           "<title>odr</title>\n"
           "</head>\n"
           "<body>\n";

    translateElement(textBody, of);

    of << "\n"
           "</body>\n"
           "</html>";

    of.close();
    return true;
}

}
