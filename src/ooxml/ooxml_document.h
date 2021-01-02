#ifndef ODR_OOXML_DOCUMENT_H
#define ODR_OOXML_DOCUMENT_H

#include <common/document.h>
#include <memory>
#include <odr/document.h>
#include <ooxml/text/ooxml_text_style.h>
#include <pugixml.hpp>

namespace odr::common {
class ReadStorage;
}

namespace odr::ooxml {

class OfficeOpenXmlDocument : public virtual common::Document {
public:
  explicit OfficeOpenXmlDocument(std::shared_ptr<common::ReadStorage> storage);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  DocumentType documentType() const noexcept final;

  std::shared_ptr<common::ReadStorage> storage() const noexcept;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const std::string &password) const final;

protected:
  std::shared_ptr<common::ReadStorage> m_storage;
  DocumentMeta m_document_meta;
};

class OfficeOpenXmlTextDocument final
    : public OfficeOpenXmlDocument,
      public common::TextDocument,
      public std::enable_shared_from_this<OfficeOpenXmlTextDocument> {
public:
  explicit OfficeOpenXmlTextDocument(
      std::shared_ptr<common::ReadStorage> storage);

  DocumentMeta documentMeta() const noexcept final;

  const text::Styles &styles() const noexcept;

  std::shared_ptr<const common::Element> root() const final;

  std::shared_ptr<common::PageStyle> pageStyle() const final;

private:
  pugi::xml_document m_documentXml;
  pugi::xml_document m_stylesXml;

  text::Styles m_styles;
};

class OfficeOpenXmlPresentation final : public OfficeOpenXmlDocument,
                                        public common::Presentation {
public:
  explicit OfficeOpenXmlPresentation(
      std::shared_ptr<common::ReadStorage> storage);

  DocumentMeta documentMeta() const noexcept final;

  std::uint32_t slideCount() const final;

  std::shared_ptr<const common::Element> root() const final;

  std::shared_ptr<const common::Slide> firstSlide() const final;

private:
  pugi::xml_document m_presentationXml;
};

class OfficeOpenXmlSpreadsheet final : public OfficeOpenXmlDocument,
                                       public common::Spreadsheet {
public:
  explicit OfficeOpenXmlSpreadsheet(
      std::shared_ptr<common::ReadStorage> storage);

  DocumentMeta documentMeta() const noexcept final;

  std::uint32_t sheetCount() const final;

  std::shared_ptr<const common::Element> root() const final;

  std::shared_ptr<const common::Sheet> firstSheet() const final;

private:
  pugi::xml_document m_workbookXml;
  pugi::xml_document m_stylesXml;
};

} // namespace odr::ooxml

#endif // ODR_OOXML_DOCUMENT_H
