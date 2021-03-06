#include <internal/abstract/file.h>
#include <internal/common/file.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/file_meta.h>
#include <open_strategy.h>
#include <utility>

namespace odr {

File::File(std::shared_ptr<internal::abstract::File> impl)
    : m_impl{std::move(impl)} {}

File::File(const std::string &path)
    : m_impl{std::make_shared<internal::common::DiscFile>(path)} {}

FileLocation File::location() const noexcept { return m_impl->location(); }

std::size_t File::size() const { return m_impl->size(); }

std::unique_ptr<std::istream> File::read() const { return m_impl->read(); }

std::shared_ptr<internal::abstract::File> File::impl() const { return m_impl; }

std::vector<FileType> DecodedFile::types(const std::string &path) {
  return open_strategy::types(
      std::make_shared<internal::common::DiscFile>(path));
}

FileType DecodedFile::type(const std::string &path) {
  return DecodedFile(path).file_type();
}

FileMeta DecodedFile::meta(const std::string &path) {
  return DecodedFile(path).file_meta();
}

DecodedFile::DecodedFile(std::shared_ptr<internal::abstract::DecodedFile> impl)
    : m_impl{std::move(impl)} {
  if (!m_impl) {
    throw FileNotFound();
  }
}

DecodedFile::DecodedFile(const std::string &path)
    : DecodedFile(open_strategy::open_file(
          std::make_shared<internal::common::DiscFile>(path))) {}

DecodedFile::DecodedFile(const std::string &path, FileType as)
    : DecodedFile(open_strategy::open_file(
          std::make_shared<internal::common::DiscFile>(path), as)) {}

FileType DecodedFile::file_type() const noexcept {
  return m_impl->file_meta().type;
}

FileCategory DecodedFile::file_category() const noexcept {
  return FileMeta::category_by_type(file_type());
}

FileMeta DecodedFile::file_meta() const noexcept { return m_impl->file_meta(); }

ImageFile DecodedFile::image_file() const {
  auto imageFile =
      std::dynamic_pointer_cast<internal::abstract::ImageFile>(m_impl);
  if (!imageFile) {
    throw NoImageFile();
  }
  return ImageFile(imageFile);
}

DocumentFile DecodedFile::document_file() const {
  auto documentFile =
      std::dynamic_pointer_cast<internal::abstract::DocumentFile>(m_impl);
  if (!documentFile) {
    throw NoDocumentFile();
  }
  return DocumentFile(documentFile);
}

ImageFile::ImageFile(std::shared_ptr<internal::abstract::ImageFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

FileType DocumentFile::type(const std::string &path) {
  return DocumentFile(path).file_type();
}

FileMeta DocumentFile::meta(const std::string &path) {
  return DocumentFile(path).file_meta();
}

DocumentFile::DocumentFile(
    std::shared_ptr<internal::abstract::DocumentFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

DocumentFile::DocumentFile(const std::string &path)
    : DocumentFile(open_strategy::open_document_file(
          std::make_shared<internal::common::DiscFile>(path))) {}

bool DocumentFile::password_encrypted() const {
  return m_impl->password_encrypted();
}

EncryptionState DocumentFile::encryption_state() const {
  return m_impl->encryption_state();
}

bool DocumentFile::decrypt(const std::string &password) {
  return m_impl->decrypt(password);
}

DocumentType DocumentFile::document_type() const {
  return m_impl->document_type();
}

DocumentMeta DocumentFile::document_meta() const {
  return m_impl->document_meta();
}

Document DocumentFile::document() const { return Document(m_impl->document()); }

} // namespace odr
