#include "ContentTranslator.h"
#include <cstring>
#include "tinyxml2.h"

namespace opendocument {

bool ContentTranslator::translate(tinyxml2::XMLElement &in, std::ostream &out) const {
}

bool TextContentTranslator::translate(tinyxml2::XMLElement &in, std::ostream &out) const {
    if (std::strcmp("text:p", in.Name()) == 0) {
        out << "<p>";
    }

    for (auto child = in.FirstChild(); child != nullptr; child = child->NextSibling()) {
        if (child->ToText() != nullptr) {
            out << child->ToText()->Value();
        } else if (child->ToElement() != nullptr) {
            translate(*child->ToElement(), out);
        }
    }

    if (std::strcmp("text:p", in.Name()) == 0) {
        out << "</p>";
    }
}

}
