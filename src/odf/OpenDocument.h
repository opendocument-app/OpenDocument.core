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

class PossiblyEncryptedOpenDocumentFile;

class OpenDocumentFile final : public virtual common::DocumentFile {
public:
  FileType fileType() const noexcept final;
  FileCategory fileCategory() const noexcept final;
  FileMeta fileMeta() const noexcept final;

  DocumentType documentType() const noexcept final;
  DocumentMeta documentMeta() const noexcept final;

  std::shared_ptr<common::Document> document() const final;

protected:
  std::unique_ptr<access::ReadStorage> m_storage;

  FileMeta m_meta;
  Meta::Manifest m_manifest;
  pugi::xml_document m_content;

private:
  bool m_decrypted{false};

  explicit OpenDocumentFile(std::unique_ptr<access::ReadStorage> &storage);

  bool decrypt(const std::string &password);

  friend PossiblyEncryptedOpenDocumentFile;
};

class PossiblyEncryptedOpenDocumentFile final : common::PossiblyPasswordEncryptedFile<OpenDocumentFile> {
public:
  FileMeta fileMeta() const noexcept final;
  EncryptionState encryptionState() const final;

  bool decrypt(const std::string &password) final;

  std::shared_ptr<OpenDocumentFile> unbox() final;

private:
  OpenDocumentFile m_documentFile;

  explicit PossiblyEncryptedOpenDocumentFile(OpenDocumentFile &&documentFile);
};

class OpenDocument : public virtual common::Document {
public:
  bool savable(bool encrypted) const noexcept final;

  DocumentType documentType() const noexcept final;
  DocumentMeta documentMeta() const noexcept final;

  void save(const access::Path &path) const final;
  void save(const access::Path &path, const std::string &password) const final;

protected:
  std::shared_ptr<OpenDocumentFile> m_file;
};

class OpenDocumentText final : public OpenDocument, public common::TextDocument {
public:
  PageProperties pageProperties() const final;

  ElementSiblingRange content() const final;

private:
  explicit OpenDocumentText(OpenDocument &&document);
};

class OpenDocumentPresentation final : public OpenDocument, public common::Presentation {
public:
  ElementSiblingRange slideContent(std::uint32_t index) const final;

private:
  explicit OpenDocumentPresentation(OpenDocument &&document);
};

class OpenDocumentSpreadsheet final : public OpenDocument, public common::Spreadsheet {
public:
  Table sheetTable(std::uint32_t index) const final;

private:
  explicit OpenDocumentSpreadsheet(OpenDocument &&document);
};

class OpenDocumentGraphics final : public OpenDocument, public common::Graphics {
public:
  ElementSiblingRange pageContent(std::uint32_t index) const final;

private:
  explicit OpenDocumentGraphics(OpenDocument &&document);
};

} // namespace odf
} // namespace odr

#endif // ODR_ODF_OPENDOCUMENT_H
