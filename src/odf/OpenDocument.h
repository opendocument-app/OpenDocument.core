#ifndef ODR_ODF_OPENDOCUMENT_H
#define ODR_ODF_OPENDOCUMENT_H

#include <common/Document.h>
#include <common/Encrypted.h>
#include <memory>
#include <odr/File.h>
#include <odf/Meta.h>
#include <pugixml.hpp>

namespace odr {
namespace access {
class ReadStorage;
}

namespace odf {

class PossiblyEncryptedOpenDocument;

class OpenDocument : public virtual common::Document {
public:
  bool savable(bool encrypted) const noexcept final;

  const FileMeta &meta() const noexcept final;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

protected:
  std::unique_ptr<access::ReadStorage> m_storage;

  FileMeta m_meta;
  Meta::Manifest m_manifest;
  pugi::xml_document m_content;

private:
  bool m_decrypted{false};

  explicit OpenDocument(std::unique_ptr<access::ReadStorage> &storage);

  bool decrypt(const std::string &password);

  friend PossiblyEncryptedOpenDocument;
};

class PossiblyEncryptedOpenDocument final : common::PossiblyPasswordEncryptedFile<OpenDocument> {
public:
  const FileMeta &meta() const noexcept final;
  EncryptionState encryptionState() const final;

  bool decrypt(const std::string &password) final;

  std::unique_ptr<OpenDocument> unbox() final;

private:
  OpenDocument m_document;

  explicit PossiblyEncryptedOpenDocument(OpenDocument &&document);
};

class OpenDocumentText final : public OpenDocument, public common::TextDocument {
public:
  PageProperties pageProperties() const final;

  ElementSiblingRange content() const final;

private:
  explicit OpenDocumentText(OpenDocument &&document);

  friend PossiblyEncryptedOpenDocument;
};

class OpenDocumentPresentation final : public OpenDocument, public common::Presentation {
public:
  ElementSiblingRange slideContent(std::uint32_t index) const final;

private:
  explicit OpenDocumentPresentation(OpenDocument &&document);

  friend PossiblyEncryptedOpenDocument;
};

class OpenDocumentSpreadsheet final : public OpenDocument, public common::Spreadsheet {
public:
  Table sheetTable(std::uint32_t index) const final;

private:
  explicit OpenDocumentSpreadsheet(OpenDocument &&document);

  friend PossiblyEncryptedOpenDocument;
};

class OpenDocumentGraphics final : public OpenDocument, public common::Graphics {
public:
  ElementSiblingRange pageContent(std::uint32_t index) const final;

private:
  explicit OpenDocumentGraphics(OpenDocument &&document);

  friend PossiblyEncryptedOpenDocument;
};

} // namespace odf
} // namespace odr

#endif // ODR_ODF_OPENDOCUMENT_H
