#ifndef ODR_OOXML_DOCUMENT_H
#define ODR_OOXML_DOCUMENT_H

#include <abstract/document.h>
#include <memory>
#include <odr/document.h>
#include <ooxml/text/ooxml_text_style.h>
#include <pugixml.hpp>

namespace odr::abstract {
class ReadableFilesystem;
}

namespace odr::ooxml {

class OfficeOpenXmlDocument : public virtual abstract::Document {
public:
  explicit OfficeOpenXmlDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  std::shared_ptr<abstract::ReadableFilesystem> filesystem() const noexcept;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const std::string &password) const final;

protected:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  DocumentMeta m_document_meta;
};

class OfficeOpenXmlTextDocument final
    : public OfficeOpenXmlDocument,
      public abstract::TextDocument,
      public std::enable_shared_from_this<OfficeOpenXmlTextDocument> {
public:
  explicit OfficeOpenXmlTextDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  DocumentMeta document_meta() const noexcept final;

  const text::Styles &styles() const noexcept;

  std::shared_ptr<const abstract::Element> root() const final;

  std::shared_ptr<abstract::PageStyle> page_style() const final;

private:
  pugi::xml_document m_document_xml;
  pugi::xml_document m_styles_xml;

  text::Styles m_styles;
};

class OfficeOpenXmlPresentation final : public OfficeOpenXmlDocument,
                                        public abstract::Presentation {
public:
  explicit OfficeOpenXmlPresentation(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  DocumentMeta document_meta() const noexcept final;

  std::uint32_t slide_count() const final;

  std::shared_ptr<const abstract::Element> root() const final;

  std::shared_ptr<const abstract::Slide> first_slide() const final;

private:
  pugi::xml_document m_presentation_xml;
};

class OfficeOpenXmlSpreadsheet final : public OfficeOpenXmlDocument,
                                       public abstract::Spreadsheet {
public:
  explicit OfficeOpenXmlSpreadsheet(
      std::shared_ptr<abstract::ReadableFilesystem> storage);

  DocumentMeta document_meta() const noexcept final;

  std::uint32_t sheet_count() const final;

  std::shared_ptr<const abstract::Element> root() const final;

  std::shared_ptr<const abstract::Sheet> first_sheet() const final;

private:
  pugi::xml_document m_workbook_xml;
  pugi::xml_document m_styles_xml;
};

} // namespace odr::ooxml

#endif // ODR_OOXML_DOCUMENT_H
