#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <odr/exceptions.hpp>

#include <poppler/PDFDocFactory.h>
#include <poppler/Stream.h>
#include <poppler/goo/GooString.h>

namespace odr::internal {

PopplerPdfFile::PopplerPdfFile(std::shared_ptr<DiskFile> file)
    : m_file{std::move(file)} {
  open(std::nullopt);
}

PopplerPdfFile::PopplerPdfFile(std::shared_ptr<MemoryFile> file)
    : m_file{std::move(file)} {
  open(std::nullopt);
}

void PopplerPdfFile::open(const std::optional<std::string> &password) {
  std::optional<GooString> password_goo;
  if (password.has_value()) {
    password_goo = GooString(password.value().c_str());
  }

  if (auto disk_file = std::dynamic_pointer_cast<DiskFile>(m_file)) {
    auto file_path_goo =
        std::make_unique<GooString>(disk_file->disk_path()->string().c_str());
    m_pdf_doc = std::make_shared<PDFDoc>(std::move(file_path_goo), password_goo,
                                         password_goo);
  } else if (auto memory_file = std::dynamic_pointer_cast<MemoryFile>(m_file)) {
    // `stream` is freed by `m_pdf_doc`
    auto stream = new MemStream(memory_file->memory_data(), 0,
                                memory_file->size(), Object(objNull));
    m_pdf_doc = std::make_shared<PDFDoc>(stream, password_goo, password_goo);
  } else {
    throw NoPdfFile();
  }

  if (!m_pdf_doc->isOk()) {
    if (m_pdf_doc->getErrorCode() == errEncrypted) {
      m_encryption_state = EncryptionState::encrypted;
    } else {
      throw std::runtime_error("Failed to open PDF file");
    }
  } else {
    m_encryption_state = m_pdf_doc->isEncrypted()
                             ? EncryptionState::decrypted
                             : EncryptionState::not_encrypted;
  }

  m_file_meta.type = FileType::portable_document_format;
  m_file_meta.password_encrypted = m_pdf_doc->isEncrypted();
  m_file_meta.document_meta.emplace();
  m_file_meta.document_meta->document_type = DocumentType::text;
  if (m_encryption_state != EncryptionState::encrypted) {
    m_file_meta.document_meta->entry_count = m_pdf_doc->getNumPages();
  }
}

std::shared_ptr<abstract::File> PopplerPdfFile::file() const noexcept {
  return m_file;
}

FileMeta PopplerPdfFile::file_meta() const noexcept { return m_file_meta; }

DecoderEngine PopplerPdfFile::decoder_engine() const noexcept {
  return DecoderEngine::poppler;
}

bool PopplerPdfFile::password_encrypted() const noexcept {
  return m_encryption_state == EncryptionState::encrypted ||
         m_encryption_state == EncryptionState::decrypted;
}

EncryptionState PopplerPdfFile::encryption_state() const noexcept {
  return m_encryption_state;
}

std::shared_ptr<abstract::DecodedFile>
PopplerPdfFile::decrypt(const std::string &password) const {
  if (encryption_state() != EncryptionState::encrypted) {
    throw NotEncryptedError();
  }

  auto decrypted_file = std::make_shared<PopplerPdfFile>(*this);
  try {
    decrypted_file->open(password);
  } catch (const std::exception &e) {
    throw DecryptionFailed();
  }
  if (decrypted_file->encryption_state() != EncryptionState::decrypted) {
    throw WrongPasswordError();
  }
  return decrypted_file;
}

PDFDoc &PopplerPdfFile::pdf_doc() const { return *m_pdf_doc; }

} // namespace odr::internal
