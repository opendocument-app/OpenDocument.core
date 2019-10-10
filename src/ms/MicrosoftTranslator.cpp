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

namespace {
struct Relationship {
    // type
    Path target;
};
std::unordered_map<std::string, Relationship> parseRelationships(tinyxml2::XMLDocument &rels) {
    std::unordered_map<std::string, Relationship> result;
    XmlUtil::recursiveVisitElementsWithName(rels.RootElement(), "Relationship", [&](const auto &rel) {
        const std::string rId = rel.FindAttribute("Id")->Value();
        const Relationship r = {
                .target = rel.FindAttribute("Target")->Value()
        };
        result.insert({rId, r});
    });
    return result;
}
}

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
        switch (file.getMeta().type) {
            case FileType::OFFICE_OPEN_XML_DOCUMENT: {
                tinyxml2::XMLHandle bodyHandle = tinyxml2::XMLHandle(in)
                        .FirstChildElement("w:document")
                        .FirstChildElement("w:body");
                tinyxml2::XMLElement *body = in
                        .FirstChildElement("w:document")
                        ->FirstChildElement("w:body");

                contentTranslator.translate(*body, context);
            } break;
            case FileType::OFFICE_OPEN_XML_PRESENTATION: {
                const auto rels = parseRelationships(*file.loadRelationships(file.getContentPath()));
                XmlUtil::recursiveVisitElementsWithName(in.RootElement(), "p:sldId", [&](const auto &child) {
                    const std::string rId = child.FindAttribute("r:id")->Value();
                    const auto sheet = file.loadXml(Path("ppt").join(rels.at(rId).target));
                    contentTranslator.translate(*sheet->RootElement(), context);
                });
            } break;
            case FileType::OFFICE_OPEN_XML_WORKBOOK: {
                // TODO this breaks back translation
                context.msSharedStringsDocument = file.loadXml("xl/sharedStrings.xml");
                XmlUtil::recursiveVisitElementsWithName(context.msSharedStringsDocument->RootElement(), "si", [&](const auto &child) {
                    context.msSharedStrings.push_back(&child);
                });

                const auto rels = parseRelationships(*file.loadRelationships(file.getContentPath()));
                XmlUtil::recursiveVisitElementsWithName(in.RootElement(), "sheet", [&](const auto &child) {
                    const std::string rId = child.FindAttribute("r:id")->Value();
                    const auto sheet = file.loadXml(Path("xl").join(rels.at(rId).target));
                    contentTranslator.translate(*sheet->RootElement(), context);
                });
            } break;
            default:
                throw; // TODO
        }
    }

    bool backTranslate(MicrosoftOpenXmlFile &in, const std::string &inHtml, const std::string &out, TranslationContext &context) const {
        // TODO code duplication
        // TODO exit on encrypted files
        tinyxml2::XMLDocument contentHtml;
        // TODO out-source parse html
        const std::string contentHtmlStr = FileUtil::read(inHtml);
        const auto contentHtmlStr_begin = contentHtmlStr.find("<body>");
        auto contentHtmlStr_end = contentHtmlStr.rfind("</body>");
        if ((contentHtmlStr_begin == std::string::npos) ||
            (contentHtmlStr_end == std::string::npos)) {
            return false;
        }
        contentHtmlStr_end += 7;
        tinyxml2::XMLError state = contentHtml.Parse(
                contentHtmlStr.c_str() + contentHtmlStr_begin,
                contentHtmlStr_end - contentHtmlStr_begin);
        if (state != tinyxml2::XMLError::XML_SUCCESS) {
            return false;
        }

        std::unordered_map<std::uint32_t, const char *> editedContent;
        XmlUtil::recursiveVisitElementsWithAttribute(contentHtml.RootElement(), "data-odr-cid", [&](const tinyxml2::XMLElement &element) {
            const std::uint32_t id = element.FindAttribute("data-odr-cid")->Int64Value();
            if ((element.FirstChild() == nullptr) || (element.FirstChild()->ToText() == nullptr)) return;
            editedContent[id] = element.FirstChild()->ToText()->Value();
        });

        for (auto &&e : context.textTranslation) {
            const auto editedIt = editedContent.find(e.first);
            // TODO dirty const off-cast
            if (editedIt == editedContent.end()) {
                ((tinyxml2::XMLText *) e.second)->SetValue("");
            } else {
                ((tinyxml2::XMLText *) e.second)->SetValue(editedIt->second);
            }
        }

        ZipWriter writer(out);
        in.getZipReader().visit([&](const auto &p) {
            if (p == in.getContentPath()) return;
            writer.copy(in.getZipReader(), p);
        });

        tinyxml2::XMLPrinter printer(0, true, 0);
        context.content->Print(&printer);
        writer.write(in.getContentPath())->write(printer.CStr(), printer.CStrSize() - 1);

        return true;
    }
};

MicrosoftTranslator::MicrosoftTranslator() :
        impl(std::make_unique<Impl>()) {
}

MicrosoftTranslator::~MicrosoftTranslator() = default;

bool MicrosoftTranslator::translate(MicrosoftOpenXmlFile &in, const std::string &out, TranslationContext &context) const {
    return impl->translate(in, out, context);
}

bool MicrosoftTranslator::backTranslate(MicrosoftOpenXmlFile &in, const std::string &inHtml, const std::string &out, TranslationContext &context) const {
    return impl->backTranslate(in, inHtml, out, context);
}

}
