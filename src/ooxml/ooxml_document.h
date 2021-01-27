#ifndef ODR_OOXML_DOCUMENT_H
#define ODR_OOXML_DOCUMENT_H

#include <abstract/document.h>
#include <memory>
#include <odr/document.h>
#include <ooxml/text/ooxml_text_style.h>
#include <pugixml.hpp>

namespace odr::abstract {
class ReadStorage;
}

namespace odr::ooxml {

class OfficeOpenXmlDocument : public virtual abstract::Document {
public:
  explicit OfficeOpenXmlDocument(
      std::shared_ptr<abstract::ReadStorage> storage);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  DocumentType document_type() const noexcept final;

  std::shared_ptr<abstract::ReadStorage> storage() const noexcept;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const std::string &password) const final;

protected:
  std::shared_ptr<abstract::ReadStorage> m_storage;
  DocumentMeta m_document_meta;
};

class OfficeOpenXmlTextDocument final
    : public OfficeOpenXmlDocument,
      public abstract::TextDocument,
      public std::enable_shared_from_this<OfficeOpenXmlTextDocument> {
public:
  explicit OfficeOpenXmlTextDocument(
      std::shared_ptr<abstract::ReadStorage> storage);

  DocumentMeta document_meta() const noexcept final;

  const text::Styles &styles() const noexcept;

  std::shared_ptr<const abstract::Element> root() const final;

  std::shared_ptr<abstract::PageStyle> page_style() const final;

private:
  pugi::xml_document m_documentXml;
  pugi::xml_document m_stylesXml;

  text::Styles m_styles;
};

class OfficeOpenXmlPresentation final : public OfficeOpenXmlDocument,
                                        public abstract::Presentation {
public:
  explicit OfficeOpenXmlPresentation(
      std::shared_ptr<abstract::ReadStorage> storage);

  DocumentMeta document_meta() const noexcept final;

  std::uint32_t slideCount() const final;

  std::shared_ptr<const abstract::Element> root() const final;

  std::shared_ptr<const abstract::Slide> firstSlide() const final;

private:
  pugi::xml_document m_presentationXml;
};

class OfficeOpenXmlSpreadsheet final : public OfficeOpenXmlDocument,
                                       public abstract::Spreadsheet {
public:
  explicit OfficeOpenXmlSpreadsheet(
      std::shared_ptr<abstract::ReadStorage> storage);

  DocumentMeta document_meta() const noexcept final;

  std::uint32_t sheetCount() const final;

  std::shared_ptr<const abstract::Element> root() const final;

  std::shared_ptr<const abstract::Sheet> firstSheet() const final;

private:
  pugi::xml_document m_workbookXml;
  pugi::xml_document m_stylesXml;
};

} // namespace odr::ooxml

#endif // ODR_OOXML_DOCUMENT_H
