#include <odf/ContentTranslator.h>
#include <odf/Context.h>
#include <odf/Crypto.h>
#include <access/StreamUtil.h>
#include <access/ZipStorage.h>
#include <common/Html.h>
#include <odf/OpenDocument.h>

namespace odr::odf {

OpenDocumentFile::OpenDocumentFile(std::unique_ptr<access::ReadStorage> &storage) {
    m_meta = Meta::parseFileMeta(*m_storage, false);
    m_manifest = Meta::parseManifest(*m_storage);
    m_storage = std::move(storage);
}

bool OpenDocumentFile::savable(const bool encrypted) const noexcept {
    if (encrypted)
        return false;
    return m_meta.encryptionState == EncryptionState::NOT_ENCRYPTED;
}

FileMeta OpenDocumentFile::fileMeta() const noexcept {
  return m_meta;
}

std::shared_ptr<common::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  switch (documentType()) {
  case DocumentType::TEXT:
    return std::unique_ptr<OpenDocument>(new OpenDocumentText(std::move(m_document)));
  case DocumentType::PRESENTATION:
    return std::unique_ptr<OpenDocument>(new OpenDocumentPresentation(std::move(m_document)));
  case DocumentType::SPREADSHEET:
    return std::unique_ptr<OpenDocument>(new OpenDocumentSpreadsheet(std::move(m_document)));
  case DocumentType::GRAPHICS:
    return std::unique_ptr<OpenDocument>(new OpenDocumentGraphics(std::move(m_document)));
  default:
    // TODO throw
    return nullptr;
  }
}

void OpenDocumentFile::save(const access::Path &path) const {
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

void OpenDocumentFile::save(const access::Path &path, const std::string &password) const {
  // TODO throw if not savable
}

bool OpenDocumentFile::decrypt(const std::string &password) {
    const bool success = Crypto::decrypt(m_storage, m_manifest, password);
    if (success)
        m_meta = Meta::parseFileMeta(*m_storage, true);
    return success;
}

PossiblyEncryptedOpenDocumentFile::PossiblyEncryptedOpenDocumentFile(OpenDocumentFile &&documentFile) : m_documentFile{std::move(documentFile)} {}

FileMeta PossiblyEncryptedOpenDocumentFile::fileMeta() const noexcept {
  return m_documentFile.fileMeta();
}

EncryptionState PossiblyEncryptedOpenDocumentFile::encryptionState() const {
  return m_documentFile.fileMeta().encryptionState; // TODO
}

bool PossiblyEncryptedOpenDocumentFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  return m_documentFile.decrypt(password);
}

std::shared_ptr<OpenDocumentFile> PossiblyEncryptedOpenDocumentFile::unbox() {
  // TODO throw if encrypted
  return std::make_shared<OpenDocumentFile>(m_documentFile);
}

OpenDocumentText::OpenDocumentText(OpenDocument &&document) : OpenDocument(std::move(document)) {}

OpenDocumentPresentation::OpenDocumentPresentation(OpenDocument &&document) : OpenDocument(std::move(document)) {}

OpenDocumentSpreadsheet::OpenDocumentSpreadsheet(OpenDocument &&document) : OpenDocument(std::move(document)) {}

OpenDocumentGraphics::OpenDocumentGraphics(OpenDocument &&document) : OpenDocument(std::move(document)) {}

} // namespace odr::odf
