#include <access/Path.h>
#include <access/ZipStorage.h>
#include <common/Constants.h>
#include <common/TranslationContext.h>
#include <common/XmlUtil.h>
#include <fstream>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <ooxml/OfficeOpenXmlDocumentTranslator.h>
#include <ooxml/OfficeOpenXmlMeta.h>
#include <ooxml/OfficeOpenXmlPresentationTranslator.h>
#include <ooxml/OfficeOpenXmlTranslator.h>
#include <ooxml/OfficeOpenXmlWorkbookTranslator.h>
#include <stdexcept>
#include <tinyxml2.h>

namespace odr {
namespace ooxml {

class OfficeOpenXmlTranslator::Impl {
public:
  bool translate(const std::string &outPath,
                 common::TranslationContext &context) const {
    std::ofstream out(outPath);
    if (!out.is_open())
      return false;
    context.output = &out;

    out << common::Constants::getHtmlBeginToStyle();

    generateStyle(out, context);

    out << common::Constants::getHtmlStyleToBody();

    generateContent(context);

    out << common::Constants::getHtmlBodyToScript();

    generateScript(out, context);

    out << common::Constants::getHtmlScriptToEnd();

    out.close();
    return true;
  }

  void generateStyle(std::ofstream &out,
                     common::TranslationContext &context) const {
    // default css
    out << common::Constants::getOpenDocumentDefaultCss();

    switch (context.meta->type) {
    case FileType::OFFICE_OPEN_XML_DOCUMENT: {
      const auto stylesXml =
          common::XmlUtil::parse(*context.storage, "word/styles.xml");
      const tinyxml2::XMLElement *styles = stylesXml->RootElement();
      OfficeOpenXmlDocumentTranslator::translateStyle(*styles, context);
    } break;
    case FileType::OFFICE_OPEN_XML_PRESENTATION: {
      // TODO duplication in generateContent
      const auto ppt =
          common::XmlUtil::parse(*context.storage, "ppt/presentation.xml");
      const tinyxml2::XMLElement *sizeEle =
          ppt->RootElement()->FirstChildElement("p:sldSz");
      if (sizeEle != nullptr) {
        float widthIn = sizeEle->FindAttribute("cx")->Int64Value() / 914400.0f;
        float heightIn = sizeEle->FindAttribute("cy")->Int64Value() / 914400.0f;

        out << ".slide {";
        out << "width:" << widthIn << "in;";
        out << "height:" << heightIn << "in;";
        out << "}";
      }
    } break;
    case FileType::OFFICE_OPEN_XML_WORKBOOK: {
      const auto stylesXml =
          common::XmlUtil::parse(*context.storage, "xl/styles.xml");
      const tinyxml2::XMLElement *styles = stylesXml->RootElement();
      OfficeOpenXmlWorkbookTranslator::translateStyle(*styles, context);
    } break;
    default:
      throw std::invalid_argument("file.getMeta().type");
    }
  }

  void generateScript(std::ofstream &of, common::TranslationContext &) const {
    of << common::Constants::getDefaultScript();
  }

  void generateContent(common::TranslationContext &context) const {
    switch (context.meta->type) {
    case FileType::OFFICE_OPEN_XML_DOCUMENT: {
      context.content =
          common::XmlUtil::parse(*context.storage, "word/document.xml");
      context.msRelations = OfficeOpenXmlMeta::parseRelationships(
          *context.storage, "word/document.xml");

      tinyxml2::XMLElement *body =
          context.content->FirstChildElement("w:document")
              ->FirstChildElement("w:body");

      OfficeOpenXmlDocumentTranslator::translateContent(*body, context);
    } break;
    case FileType::OFFICE_OPEN_XML_PRESENTATION: {
      const auto ppt =
          common::XmlUtil::parse(*context.storage, "ppt/presentation.xml");
      const auto pptRelations = OfficeOpenXmlMeta::parseRelationships(
          *context.storage, "ppt/presentation.xml");

      common::XmlUtil::recursiveVisitElementsWithName(
          ppt->RootElement(), "p:sldId", [&](const auto &e) {
            const std::string rId = e.FindAttribute("r:id")->Value();

            const auto path = access::Path("ppt").join(pptRelations.at(rId));
            context.content = common::XmlUtil::parse(*context.storage, path);
            context.msRelations =
                OfficeOpenXmlMeta::parseRelationships(*context.storage, path);

            if ((context.config->entryOffset > 0) ||
                (context.config->entryCount > 0)) {
              if ((context.currentEntry >= context.config->entryOffset) &&
                  (context.currentEntry <
                   context.config->entryOffset + context.config->entryCount)) {
                OfficeOpenXmlPresentationTranslator::translateContent(
                    *context.content->RootElement(), context);
              }
            } else {
              OfficeOpenXmlPresentationTranslator::translateContent(
                  *context.content->RootElement(), context);
            }

            ++context.currentEntry;
          });
    } break;
    case FileType::OFFICE_OPEN_XML_WORKBOOK: {
      const auto xls =
          common::XmlUtil::parse(*context.storage, "xl/workbook.xml");
      const auto xlsRelations = OfficeOpenXmlMeta::parseRelationships(
          *context.storage, "xl/workbook.xml");

      // TODO this breaks back translation
      context.msSharedStringsDocument =
          common::XmlUtil::parse(*context.storage, "xl/sharedStrings.xml");
      if (context.msSharedStringsDocument) {
        common::XmlUtil::recursiveVisitElementsWithName(
            context.msSharedStringsDocument->RootElement(), "si",
            [&](const auto &child) {
              context.msSharedStrings.push_back(&child);
            });
      }

      common::XmlUtil::recursiveVisitElementsWithName(
          xls->RootElement(), "sheet", [&](const auto &e) {
            const std::string rId = e.FindAttribute("r:id")->Value();

            const auto path = access::Path("xl").join(xlsRelations.at(rId));
            context.content = common::XmlUtil::parse(*context.storage, path);
            context.msRelations =
                OfficeOpenXmlMeta::parseRelationships(*context.storage, path);

            if ((context.config->entryOffset > 0) ||
                (context.config->entryCount > 0)) {
              if ((context.currentEntry >= context.config->entryOffset) &&
                  (context.currentEntry <
                   context.config->entryOffset + context.config->entryCount)) {
                OfficeOpenXmlWorkbookTranslator::translateContent(
                    *context.content->RootElement(), context);
              }
            } else {
              OfficeOpenXmlWorkbookTranslator::translateContent(
                  *context.content->RootElement(), context);
            }

            ++context.currentEntry;
          });
    } break;
    default:
      throw std::invalid_argument("file.getMeta().type");
    }
  }

  bool backTranslate(const std::string &, const std::string &,
                     common::TranslationContext &) const {
    return false;
  }
};

OfficeOpenXmlTranslator::OfficeOpenXmlTranslator()
    : impl(std::make_unique<Impl>()) {}

OfficeOpenXmlTranslator::~OfficeOpenXmlTranslator() = default;

bool OfficeOpenXmlTranslator::translate(
    const std::string &outPath, common::TranslationContext &context) const {
  return impl->translate(outPath, context);
}

bool OfficeOpenXmlTranslator::backTranslate(
    const std::string &diff, const std::string &outPath,
    common::TranslationContext &context) const {
  return impl->backTranslate(diff, outPath, context);
}

} // namespace ooxml
} // namespace odr
