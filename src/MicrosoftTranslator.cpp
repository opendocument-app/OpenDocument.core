#include "MicrosoftTranslator.h"
#include <fstream>
#include "tinyxml2.h"
#include "glog/logging.h"
#include "odr/TranslationConfig.h"
#include "MicrosoftOpenXmlFile.h"
#include "MicrosoftContentTranslator.h"
#include "TranslationContext.h"

namespace odr {

namespace {

class DefaultDocumentTranslatorImpl : public MicrosoftTranslator {
public:
    std::unique_ptr<MicrosoftContentTranslator> contentTranslator;

    ~DefaultDocumentTranslatorImpl() override = default;

    bool translate(MicrosoftOpenXmlFile &in, const std::string &out, TranslationContext &context) const override {
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
        of << "</style>\n";

        of << "<script>\n";
        of << "</script>\n";

        of << "</head>\n";
        of << "<body>\n";

        auto documentXml = in.loadXML("word/document.xml");
        tinyxml2::XMLHandle documentHandle(documentXml.get());
        generateContent(in, documentHandle, context);

        of << "\n";
        of << "</body>\n";
        of << "</html>\n";

        of.close();
        return true;
    }

    void generateContent(MicrosoftOpenXmlFile &file, tinyxml2::XMLHandle &in, TranslationContext &context) const {
        tinyxml2::XMLHandle bodyHandle = in
                .FirstChildElement("w:document")
                .FirstChildElement("w:body");
        tinyxml2::XMLElement *body = bodyHandle.ToElement();

        contentTranslator->translate(*body, context);
    }
};

}

std::unique_ptr<MicrosoftTranslator> MicrosoftTranslator::create() {
    auto result = std::make_unique<DefaultDocumentTranslatorImpl>();
    result->contentTranslator = MicrosoftContentTranslator::create();
    return result;
}

}
