#ifndef ODR_ODF_OPENDOCUMENT_H
#define ODR_ODF_OPENDOCUMENT_H

#include <odr/Document.h>
#include <common/Document.h>
#include <memory>
#include <pugixml.hpp>

namespace odr {
namespace access {
class ReadStorage;
}

namespace odf {

class OpenDocument : public virtual common::Document {
public:
  explicit OpenDocument(std::shared_ptr<access::ReadStorage> storage);

  DocumentType documentType() const noexcept final;
  DocumentMeta documentMeta() const noexcept final;

  bool savable(bool encrypted) const noexcept final;
  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

protected:
  std::shared_ptr<access::ReadStorage> m_storage;
  DocumentMeta m_document_meta;
  pugi::xml_document m_content;
};

class OpenDocumentText final : public OpenDocument,
                               public common::TextDocument {
public:
  explicit OpenDocumentText(std::shared_ptr<access::ReadStorage> storage);

  PageProperties pageProperties() const final;

  ElementSiblingRange content() const final;

private:
  pugi::xml_document m_style;
};

class OpenDocumentPresentation final : public OpenDocument,
                                       public common::Presentation {
public:
  explicit OpenDocumentPresentation(std::shared_ptr<access::ReadStorage> storage);

  ElementSiblingRange slideContent(std::uint32_t index) const final;
};

class OpenDocumentSpreadsheet final : public OpenDocument,
                                      public common::Spreadsheet {
public:
  explicit OpenDocumentSpreadsheet(std::shared_ptr<access::ReadStorage> storage);

  Table sheetTable(std::uint32_t index) const final;
};

class OpenDocumentGraphics final : public OpenDocument,
                                   public common::Graphics {
public:
  explicit OpenDocumentGraphics(std::shared_ptr<access::ReadStorage> storage);

  ElementSiblingRange pageContent(std::uint32_t index) const final;
};

} // namespace odf
} // namespace odr

#endif // ODR_ODF_OPENDOCUMENT_H
