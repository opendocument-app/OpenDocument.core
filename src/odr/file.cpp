#include <odr/file.hpp>

#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <optional>

namespace odr {

DocumentMeta::DocumentMeta() = default;

DocumentMeta::DocumentMeta(const DocumentType document_type,
                           const std::optional<std::uint32_t> entry_count)
    : document_type{document_type}, entry_count{entry_count} {}

FileMeta::FileMeta() = default;

FileMeta::FileMeta(const FileType type, const bool password_encrypted,
                   const std::optional<DocumentMeta> document_meta)
    : type{type}, password_encrypted{password_encrypted},
      document_meta{document_meta} {}

File::File() = default;

File::File(std::shared_ptr<internal::abstract::File> impl)
    : m_impl{std::move(impl)} {}

File::File(const std::string &path)
    : m_impl{std::make_shared<internal::DiskFile>(path)} {}

FileLocation File::location() const noexcept { return m_impl->location(); }

std::size_t File::size() const { return m_impl->size(); }

std::optional<std::string> File::disk_path() const {
  if (const std::optional<internal::AbsPath> path = m_impl->disk_path()) {
    return path->string();
  }
  return {};
}

const char *File::memory_data() const { return m_impl->memory_data(); }

std::unique_ptr<std::istream> File::stream() const { return m_impl->stream(); }

void File::pipe(std::ostream &out) const {
  internal::util::stream::pipe(*stream(), out);
}

void File::copy(const std::string &path) const {
  internal::util::file::write(*stream(), path);
}

std::shared_ptr<internal::abstract::File> File::impl() const { return m_impl; }

std::vector<FileType> DecodedFile::list_file_types(const std::string &path,
                                                   Logger &logger) {
  return internal::open_strategy::list_file_types(
      std::make_shared<internal::DiskFile>(path), logger);
}

std::vector<DecoderEngine>
DecodedFile::list_decoder_engines(const FileType as) {
  return internal::open_strategy::list_decoder_engines(as);
}

DecodedFile::DecodedFile(std::shared_ptr<internal::abstract::DecodedFile> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw NullPointerError("impl");
  }
}

DecodedFile::DecodedFile(const File &file, Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(file.impl(), logger)) {}

DecodedFile::DecodedFile(const File &file, const FileType as, Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(file.impl(), as, logger)) {
}

DecodedFile::DecodedFile(const std::string &path, Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(
          std::make_shared<internal::DiskFile>(path), logger)) {}

DecodedFile::DecodedFile(const std::string &path, const FileType as,
                         Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(
          std::make_shared<internal::DiskFile>(path), as, logger)) {}

DecodedFile::DecodedFile(const std::string &path,
                         const DecodePreference &preference, Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(
          std::make_shared<internal::DiskFile>(path), preference, logger)) {}

File DecodedFile::file() const { return File(m_impl->file()); }

FileType DecodedFile::file_type() const noexcept { return m_impl->file_type(); }

FileCategory DecodedFile::file_category() const noexcept {
  return m_impl->file_category();
}

FileMeta DecodedFile::file_meta() const noexcept { return m_impl->file_meta(); }

DecoderEngine DecodedFile::decoder_engine() const noexcept {
  return m_impl->decoder_engine();
}

bool DecodedFile::password_encrypted() const {
  return m_impl->password_encrypted();
}

EncryptionState DecodedFile::encryption_state() const {
  return m_impl->encryption_state();
}

DecodedFile DecodedFile::decrypt(const std::string &password) const {
  return DecodedFile(m_impl->decrypt(password));
}

bool DecodedFile::is_text_file() const {
  return std::dynamic_pointer_cast<internal::abstract::TextFile>(m_impl) !=
         nullptr;
}

bool DecodedFile::is_image_file() const {
  return std::dynamic_pointer_cast<internal::abstract::ImageFile>(m_impl) !=
         nullptr;
}

bool DecodedFile::is_archive_file() const {
  return std::dynamic_pointer_cast<internal::abstract::ArchiveFile>(m_impl) !=
         nullptr;
}

bool DecodedFile::is_document_file() const {
  return std::dynamic_pointer_cast<internal::abstract::DocumentFile>(m_impl) !=
         nullptr;
}

bool DecodedFile::is_pdf_file() const {
  return std::dynamic_pointer_cast<internal::abstract::PdfFile>(m_impl) !=
         nullptr;
}

TextFile DecodedFile::as_text_file() const {
  if (const auto text_file =
          std::dynamic_pointer_cast<internal::abstract::TextFile>(m_impl)) {
    return TextFile(text_file);
  }
  throw NoTextFile();
}

ImageFile DecodedFile::as_image_file() const {
  if (const auto image_file =
          std::dynamic_pointer_cast<internal::abstract::ImageFile>(m_impl)) {
    return ImageFile(image_file);
  }
  throw NoImageFile();
}

ArchiveFile DecodedFile::as_archive_file() const {
  if (const auto archive_file =
          std::dynamic_pointer_cast<internal::abstract::ArchiveFile>(m_impl)) {
    return ArchiveFile(archive_file);
  }
  throw NoArchiveFile();
}

DocumentFile DecodedFile::as_document_file() const {
  if (const auto document_file =
          std::dynamic_pointer_cast<internal::abstract::DocumentFile>(m_impl)) {
    return DocumentFile(document_file);
  }
  throw NoDocumentFile();
}

PdfFile DecodedFile::as_pdf_file() const {
  if (const auto pdf_file =
          std::dynamic_pointer_cast<internal::abstract::PdfFile>(m_impl)) {
    return PdfFile(pdf_file);
  }
  throw NoPdfFile();
}

TextFile::TextFile(std::shared_ptr<internal::abstract::TextFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

std::optional<std::string> TextFile::charset() const {
  return {}; // TODO
}

std::unique_ptr<std::istream> TextFile::stream() const {
  return m_impl->file()->stream();
}

std::string TextFile::text() const {
  return ""; // TODO
}

ImageFile::ImageFile(std::shared_ptr<internal::abstract::ImageFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

std::unique_ptr<std::istream> ImageFile::stream() const {
  return m_impl->file()->stream();
}

ArchiveFile::ArchiveFile(std::shared_ptr<internal::abstract::ArchiveFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

Archive ArchiveFile::archive() const { return Archive(m_impl->archive()); }

FileType DocumentFile::type(const std::string &path) {
  return DocumentFile(path).file_type();
}

FileMeta DocumentFile::meta(const std::string &path) {
  return DocumentFile(path).file_meta();
}

DocumentFile::DocumentFile(
    std::shared_ptr<internal::abstract::DocumentFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

DocumentFile::DocumentFile(const std::string &path, Logger &logger)
    : DocumentFile(internal::open_strategy::open_document_file(
          std::make_shared<internal::DiskFile>(path), logger)) {}

DocumentType DocumentFile::document_type() const {
  return m_impl->document_type();
}

DocumentMeta DocumentFile::document_meta() const {
  return m_impl->document_meta();
}

DocumentFile DocumentFile::decrypt(const std::string &password) const {
  return DecodedFile::decrypt(password).as_document_file();
}

Document DocumentFile::document() const { return Document(m_impl->document()); }

std::shared_ptr<internal::abstract::DocumentFile> DocumentFile::impl() const {
  return m_impl;
}

PdfFile::PdfFile(std::shared_ptr<internal::abstract::PdfFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

PdfFile PdfFile::decrypt(const std::string &password) const {
  return DecodedFile::decrypt(password).as_pdf_file();
}

std::shared_ptr<internal::abstract::PdfFile> PdfFile::impl() const {
  return m_impl;
}

} // namespace odr
