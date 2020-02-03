#ifndef ODR_OFFICEOPENXMLWORKBOOKTRANSLATOR_H
#define ODR_OFFICEOPENXMLWORKBOOKTRANSLATOR_H

#include <memory>

namespace tinyxml2 {
class XMLElement;
}

namespace odr {

struct TranslationContext;

class OfficeOpenXmlWorkbookTranslator {
public:
    static void translateStyle(const tinyxml2::XMLElement &in, TranslationContext &context);
    static void translateContent(const tinyxml2::XMLElement &in, TranslationContext &context);
};

}

#endif //ODR_OFFICEOPENXMLWORKBOOKTRANSLATOR_H
