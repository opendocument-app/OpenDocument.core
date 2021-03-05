#ifndef ODR_ODF_DOCUMENT_H
#define ODR_ODF_DOCUMENT_H

#include <abstract/document.h>
#include <memory>
#include <odf/odf_style.h>
#include <odr/document.h>
#include <pugixml.hpp>

namespace odr::abstract {
class ReadableFilesystem;
}

namespace odr::odf {

class OpenDocument : public virtual abstract::Document,
                     public std::enable_shared_from_this<OpenDocument> {
public:
  explicit OpenDocument(std::shared_ptr<abstract::ReadableFilesystem> files);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  std::shared_ptr<abstract::ReadableFilesystem> filesystem() const noexcept;
  const Styles &styles() const noexcept;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const std::string &password) const final;

protected:
  std::shared_ptr<abstract::ReadableFilesystem> m_files;
  pugi::xml_document m_content_xml;
  pugi::xml_document m_styles_xml;
  Styles m_styles;
};

class OpenDocumentText final : public OpenDocument,
                               public abstract::TextDocument {
public:
  explicit OpenDocumentText(
      std::shared_ptr<abstract::ReadableFilesystem> files);

  DocumentMeta document_meta() const noexcept final;

  std::shared_ptr<const abstract::Element> root() const final;

  std::shared_ptr<abstract::PageStyle> page_style() const final;
};

class OpenDocumentPresentation final : public OpenDocument,
                                       public abstract::Presentation {
public:
  explicit OpenDocumentPresentation(
      std::shared_ptr<abstract::ReadableFilesystem> files);

  DocumentMeta document_meta() const noexcept final;

  std::uint32_t slide_count() const final;

  std::shared_ptr<const abstract::Element> root() const final;
  std::shared_ptr<const abstract::Slide> first_slide() const final;
};

class OpenDocumentSpreadsheet final : public OpenDocument,
                                      public abstract::Spreadsheet {
public:
  explicit OpenDocumentSpreadsheet(
      std::shared_ptr<abstract::ReadableFilesystem> files);

  DocumentMeta document_meta() const noexcept final;

  std::uint32_t sheet_count() const final;

  std::shared_ptr<const abstract::Element> root() const final;
  std::shared_ptr<const abstract::Sheet> first_sheet() const final;
};

class OpenDocumentDrawing final : public OpenDocument,
                                  public abstract::Drawing {
public:
  explicit OpenDocumentDrawing(
      std::shared_ptr<abstract::ReadableFilesystem> files);

  DocumentMeta document_meta() const noexcept final;

  std::uint32_t page_count() const final;

  std::shared_ptr<const abstract::Element> root() const final;
  std::shared_ptr<const abstract::Page> first_page() const final;
};

} // namespace odr::odf

#endif // ODR_ODF_DOCUMENT_H
