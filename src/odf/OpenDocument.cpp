#include <access/ZipStorage.h>
#include <access/StreamUtil.h>
#include <common/XmlUtil.h>
#include <odf/Crypto.h>
#include <odf/OpenDocument.h>

namespace odr::odf {

OpenDocumentFile::OpenDocumentFile(
    std::shared_ptr<access::ReadStorage> storage) {
  if (!storage->isFile("META-INF/manifest.xml")) {
    auto manifest = common::XmlUtil::parse(*storage, "META-INF/manifest.xml");

    m_file_meta = parseFileMeta(*m_storage, &manifest);
    m_manifest = parseManifest(manifest);
  } else {
    m_file_meta = parseFileMeta(*m_storage, nullptr);
  }

  if (m_file_meta.passwordEncrypted)
    m_encryptionState = EncryptionState::ENCRYPTED;

  m_storage = std::move(storage);
}

EncryptionState OpenDocumentFile::encryptionState() const noexcept {
  return m_encryptionState;
}

bool OpenDocumentFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted or already decrypted
  return Crypto::decrypt(m_storage, m_manifest, password);
}

FileType OpenDocumentFile::fileType() const noexcept {
  return m_file_meta.type;
}

FileMeta OpenDocumentFile::fileMeta() const noexcept { return m_file_meta; }

std::shared_ptr<common::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  switch (documentType()) {
  case DocumentType::TEXT:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentText(m_storage));
  case DocumentType::PRESENTATION:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentPresentation(m_storage));
  case DocumentType::SPREADSHEET:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentSpreadsheet(m_storage));
  case DocumentType::GRAPHICS:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentGraphics(m_storage));
  default:
    // TODO throw
    return nullptr;
  }
}

OpenDocument::OpenDocument(std::shared_ptr<access::ReadStorage> storage)
    : m_storage{std::move(storage)} {
  m_content = common::XmlUtil::parse(*storage, "content.xml");

  if (!storage->isFile("meta.xml")) {
    auto meta = common::XmlUtil::parse(*storage, "meta.xml");
    m_document_meta = parseDocumentMeta(&meta, m_content);
  } else {
    m_document_meta = parseDocumentMeta(nullptr, m_content);
  }
}

DocumentType OpenDocument::documentType() const noexcept {
  return m_document_meta.documentType;
}

DocumentMeta OpenDocument::documentMeta() const noexcept {
  return m_document_meta;
}

void OpenDocument::save(const access::Path &path) const {
  // TODO throw if not savable
  // TODO this would decrypt/inflate and encrypt/deflate again
  access::ZipWriter writer(path);

  // `mimetype` has to be the first file and uncompressed
  if (m_storage->isFile("mimetype")) {
    const auto in = m_storage->read("mimetype");
    const auto out = writer.write("mimetype", 0);
    access::StreamUtil::pipe(*in, *out);
  }

  m_storage->visit([&](const auto &p) {
    std::cout << p.string() << std::endl;
    if (p == "mimetype")
      return;
    if (m_storage->isDirectory(p)) {
      writer.createDirectory(p);
      return;
    }
    const auto in = m_storage->read(p);
    const auto out = writer.write(p);
    if (p == "content.xml") {
      m_content.print(*out);
      return;
    }
    access::StreamUtil::pipe(*in, *out);
  });
}

void OpenDocument::save(const access::Path &path,
                        const std::string &password) const {
  // TODO throw if not savable
}

OpenDocumentText::OpenDocumentText(std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

OpenDocumentPresentation::OpenDocumentPresentation(std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

OpenDocumentSpreadsheet::OpenDocumentSpreadsheet(std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

OpenDocumentGraphics::OpenDocumentGraphics(std::shared_ptr<access::ReadStorage> storage)
    : OpenDocument(std::move(storage)) {}

} // namespace odr::odf
