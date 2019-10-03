#include "MicrosoftTranslator.h"
#include <fstream>
#include "tinyxml2.h"
#include "odr/FileMeta.h"
#include "odr/TranslationConfig.h"
#include "../TranslationContext.h"
#include "../io/Path.h"
#include "../io/FileUtil.h"
#include "../io/ZipStorage.h"
#include "../xml/XmlUtil.h"
#include "MicrosoftOpenXmlFile.h"
#include "MicrosoftContentTranslator.h"

namespace odr {

class MicrosoftTranslator::Impl {
public:
    MicrosoftContentTranslator contentTranslator;

    bool translate(MicrosoftOpenXmlFile &in, const std::string &out, TranslationContext &context) const {
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
        of << "</style>\n";

        of << "<script>\n";
        generateScript(of, context);
        of << "</script>\n";

        of << "</head>\n";
        of << "<body>\n";

        context.content = in.loadContent();
        generateContent(in, *context.content, context);

        of << "</body>\n";
        of << "</html>\n";

        of.close();
        return true;
    }

    void generateStyle(std::ofstream &of, TranslationContext &context) const {
    }

    void generateScript(std::ofstream &of, TranslationContext &context) const {
    }

    void generateContent(MicrosoftOpenXmlFile &file, tinyxml2::XMLDocument &in, TranslationContext &context) const {
        tinyxml2::XMLHandle bodyHandle = tinyxml2::XMLHandle(in)
                .FirstChildElement("w:document")
                .FirstChildElement("w:body");
        tinyxml2::XMLElement *body = in
                .FirstChildElement("w:document")
                ->FirstChildElement("w:body");

        contentTranslator.translate(*body, context);
    }
};

MicrosoftTranslator::MicrosoftTranslator() :
        impl(std::make_unique<Impl>()) {
}

MicrosoftTranslator::~MicrosoftTranslator() = default;

bool MicrosoftTranslator::translate(MicrosoftOpenXmlFile &in, const std::string &out, TranslationContext &context) const {
    return impl->translate(in, out, context);
}

}
