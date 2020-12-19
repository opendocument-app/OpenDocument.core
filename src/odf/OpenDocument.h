#ifndef ODR_ODF_OPENDOCUMENT_H
#define ODR_ODF_OPENDOCUMENT_H

#include <common/Document.h>
#include <memory>
#include <odf/Styles.h>
#include <odr/Document.h>
#include <pugixml.hpp>

namespace odr {
namespace access {
class ReadStorage;
}

namespace odf {

class OpenDocument : public virtual common::Document,
                     public std::enable_shared_from_this<OpenDocument> {
public:
  explicit OpenDocument(std::shared_ptr<access::ReadStorage> storage);

  bool editable() const noexcept final;
  bool savable(bool encrypted) const noexcept final;

  DocumentType documentType() const noexcept final;
  DocumentMeta documentMeta() const noexcept final;

  std::shared_ptr<access::ReadStorage> storage() const noexcept;
  const Styles &styles() const noexcept;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

protected:
  std::shared_ptr<access::ReadStorage> m_storage;
  DocumentMeta m_document_meta;
  pugi::xml_document m_contentXml;
  pugi::xml_document m_stylesXml;
  Styles m_styles;
};

class OpenDocumentText final : public OpenDocument,
                               public common::TextDocument {
public:
  explicit OpenDocumentText(std::shared_ptr<access::ReadStorage> storage);

  std::shared_ptr<const common::Element> root() const final;

  PageProperties pageProperties() const final;
};

class OpenDocumentPresentation final : public OpenDocument,
                                       public common::Presentation {
public:
  explicit OpenDocumentPresentation(
      std::shared_ptr<access::ReadStorage> storage);

  std::uint32_t slideCount() const final;

  std::shared_ptr<const common::Element> root() const final;
  std::shared_ptr<const common::Slide> firstSlide() const final;
};

class OpenDocumentSpreadsheet final : public OpenDocument,
                                      public common::Spreadsheet {
public:
  explicit OpenDocumentSpreadsheet(
      std::shared_ptr<access::ReadStorage> storage);

  std::uint32_t sheetCount() const final;

  std::shared_ptr<const common::Element> root() const final;
  std::shared_ptr<const common::Sheet> firstSheet() const final;
};

class OpenDocumentDrawing final : public OpenDocument, public common::Drawing {
public:
  explicit OpenDocumentDrawing(std::shared_ptr<access::ReadStorage> storage);

  std::uint32_t pageCount() const final;

  std::shared_ptr<const common::Element> root() const final;
  std::shared_ptr<const common::Page> firstPage() const final;
};

} // namespace odf
} // namespace odr

#endif // ODR_ODF_OPENDOCUMENT_H
