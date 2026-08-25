#include <odr/file.hpp>

#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/csv/csv_file.hpp>
#include <odr/internal/encoding/transcode.hpp>
#include <odr/internal/magic.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <optional>

namespace odr {

namespace {

/// The other constructors reject a null impl, so only a default-constructed
/// @ref File reaches the throw.
const internal::abstract::File &
deref(const std::shared_ptr<internal::abstract::File> &impl) {
  if (impl == nullptr) {
    throw NullPointerError("impl");
  }
  return *impl;
}

} // namespace

File File::from_disk(const std::string &path) {
  return File(std::make_shared<internal::DiskFile>(path));
}

File File::from_memory(std::string data) {
  return File(std::make_shared<internal::MemoryFile>(std::move(data)));
}

File::File() = default;

File::File(std::shared_ptr<internal::abstract::File> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw NullPointerError("impl");
  }
}

File::File(const std::string &path)
    : m_impl{std::make_shared<internal::DiskFile>(path)} {}

/// `noexcept` leaves no way to report a null impl, hence `unknown`.
FileLocation File::location() const noexcept {
  return m_impl == nullptr ? FileLocation::unknown : m_impl->location();
}

std::size_t File::size() const { return deref(m_impl).size(); }

std::optional<std::string> File::disk_path() const {
  if (const std::optional<internal::AbsPath> path = deref(m_impl).disk_path()) {
    return path->string();
  }
  return {};
}

std::optional<std::string_view> File::memory_data() const {
  return deref(m_impl).memory_data();
}

std::unique_ptr<std::istream> File::stream() const {
  return deref(m_impl).stream();
}

void File::pipe(std::ostream &out) const {
  internal::util::stream::pipe(*stream(), out);
}

void File::copy(const std::string &path) const {
  internal::util::file::write(*stream(), path);
}

std::shared_ptr<internal::abstract::File> File::impl() const { return m_impl; }

std::vector<FileType> DecodedFile::list_file_types(const File &file,
                                                   const Logger &logger) {
  return internal::open_strategy::list_file_types(file.impl(), logger);
}

std::vector<FileType> DecodedFile::list_file_types(const std::string &path,
                                                   const Logger &logger) {
  return list_file_types(File::from_disk(path), logger);
}

std::string_view DecodedFile::mimetype(const File &file, const Logger &logger) {
  return internal::magic::mimetype(file.impl(), logger);
}

std::string_view DecodedFile::mimetype(const std::string &path,
                                       const Logger &logger) {
  return mimetype(File::from_disk(path), logger);
}

DecodedFile::DecodedFile(std::shared_ptr<internal::abstract::DecodedFile> impl)
    : m_impl{std::move(impl)} {
  if (m_impl == nullptr) {
    throw NullPointerError("impl");
  }
}

DecodedFile::DecodedFile(const File &file, const Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(file.impl(), logger)) {}

DecodedFile::DecodedFile(const File &file, const FileType as,
                         const Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(file.impl(), as, logger)) {
}

DecodedFile::DecodedFile(const File &file, const DecodePreference &preference,
                         const Logger &logger)
    : DecodedFile(internal::open_strategy::open_file(file.impl(), preference,
                                                     logger)) {}

DecodedFile::DecodedFile(const std::string &path, const Logger &logger)
    : DecodedFile(File::from_disk(path), logger) {}

DecodedFile::DecodedFile(const std::string &path, const FileType as,
                         const Logger &logger)
    : DecodedFile(File::from_disk(path), as, logger) {}

DecodedFile::DecodedFile(const std::string &path,
                         const DecodePreference &preference,
                         const Logger &logger)
    : DecodedFile(File::from_disk(path), preference, logger) {}

File DecodedFile::file() const { return File(m_impl->file()); }

FileType DecodedFile::file_type() const noexcept { return m_impl->file_type(); }

FileCategory DecodedFile::file_category() const noexcept {
  return m_impl->file_category();
}

FileMeta DecodedFile::file_meta() const noexcept { return m_impl->file_meta(); }

bool DecodedFile::password_encrypted() const {
  return m_impl->password_encrypted();
}

EncryptionState DecodedFile::encryption_state() const {
  return m_impl->encryption_state();
}

DecodedFile DecodedFile::decrypt(const std::string &password) const {
  return DecodedFile(m_impl->decrypt(password));
}

bool DecodedFile::is_decodable() const noexcept {
  return m_impl->is_decodable();
}

FileTypeCapabilities DecodedFile::capabilities() const {
  FileTypeCapabilities result = capabilities_by_file_type(file_type());

  // we are holding the decoded file, so this one is settled
  result.open = true;
  // only an actually encrypted file can be decrypted
  result.decrypt =
      result.decrypt && encryption_state() == EncryptionState::encrypted;
  // an encrypted file has to be decrypted before it renders
  result.translate_html =
      result.translate_html && encryption_state() != EncryptionState::encrypted;
  // there is no scheme without html
  result.color_scheme = result.color_scheme && result.translate_html;

  // `edit`/`save`/`encrypt` stay as declared — resolving them would mean
  // decoding the document; ask `Document` for the precise answer

  return result;
}

bool DecodedFile::is_text_file() const {
  return std::dynamic_pointer_cast<internal::abstract::TextFile>(m_impl) !=
         nullptr;
}

bool DecodedFile::is_csv_file() const {
  return std::dynamic_pointer_cast<internal::abstract::CsvFile>(m_impl) !=
         nullptr;
}

bool DecodedFile::is_markdown_file() const {
  return std::dynamic_pointer_cast<internal::abstract::MarkdownFile>(m_impl) !=
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

bool DecodedFile::is_font_file() const {
  return std::dynamic_pointer_cast<internal::abstract::FontFile>(m_impl) !=
         nullptr;
}

TextFile DecodedFile::as_text_file() const {
  if (const std::shared_ptr text_file =
          std::dynamic_pointer_cast<internal::abstract::TextFile>(m_impl);
      text_file != nullptr) {
    return TextFile(text_file);
  }
  throw NoTextFile();
}

CsvFile DecodedFile::as_csv_file() const {
  if (const std::shared_ptr csv_file =
          std::dynamic_pointer_cast<internal::abstract::CsvFile>(m_impl);
      csv_file != nullptr) {
    return CsvFile(csv_file);
  }
  throw NoCsvFile();
}

MarkdownFile DecodedFile::as_markdown_file() const {
  if (const std::shared_ptr markdown_file =
          std::dynamic_pointer_cast<internal::abstract::MarkdownFile>(m_impl);
      markdown_file != nullptr) {
    return MarkdownFile(markdown_file);
  }
  throw NoMarkdownFile();
}

ImageFile DecodedFile::as_image_file() const {
  if (const std::shared_ptr image_file =
          std::dynamic_pointer_cast<internal::abstract::ImageFile>(m_impl);
      image_file != nullptr) {
    return ImageFile(image_file);
  }
  throw NoImageFile();
}

ArchiveFile DecodedFile::as_archive_file() const {
  if (const std::shared_ptr archive_file =
          std::dynamic_pointer_cast<internal::abstract::ArchiveFile>(m_impl);
      archive_file != nullptr) {
    return ArchiveFile(archive_file);
  }
  throw NoArchiveFile();
}

DocumentFile DecodedFile::as_document_file() const {
  if (const std::shared_ptr document_file =
          std::dynamic_pointer_cast<internal::abstract::DocumentFile>(m_impl);
      document_file != nullptr) {
    return DocumentFile(document_file);
  }
  throw NoDocumentFile();
}

PdfFile DecodedFile::as_pdf_file() const {
  if (const std::shared_ptr pdf_file =
          std::dynamic_pointer_cast<internal::abstract::PdfFile>(m_impl);
      pdf_file != nullptr) {
    return PdfFile(pdf_file);
  }
  throw NoPdfFile();
}

FontFile DecodedFile::as_font_file() const {
  if (const std::shared_ptr font_file =
          std::dynamic_pointer_cast<internal::abstract::FontFile>(m_impl);
      font_file != nullptr) {
    return FontFile(font_file);
  }
  throw NoFontFile();
}

TextFile::TextFile(std::shared_ptr<internal::abstract::TextFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

TextEncoding TextFile::encoding() const { return m_impl->encoding(); }

std::optional<std::string> TextFile::charset() const {
  const TextEncoding encoding = this->encoding();
  if (encoding == TextEncoding::unknown) {
    return {};
  }
  return std::string(text_encoding_to_string(encoding));
}

std::unique_ptr<std::istream> TextFile::stream() const {
  return m_impl->file()->stream();
}

std::string TextFile::text() const {
  const auto stream = m_impl->file()->stream();
  std::string bytes = internal::util::stream::read(*stream);

  const TextEncoding encoding = this->encoding();
  if (!text_encoding_is_decodable(encoding)) {
    return bytes;
  }
  return internal::encoding::to_utf8(bytes, encoding);
}

std::shared_ptr<internal::abstract::TextFile> TextFile::impl() const {
  return m_impl;
}

CsvFile CsvFile::from_file(const File &file, const CsvOptions &options,
                           const Logger &logger) {
  ODR_VERBOSE(logger, "open as csv with options");
  return CsvFile(
      std::make_shared<internal::csv::CsvFile>(file.impl(), options));
}

CsvFile::CsvFile(std::shared_ptr<internal::abstract::CsvFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

Document CsvFile::document() const { return Document(m_impl->document()); }

CsvOptions CsvFile::options() const { return m_impl->options(); }

CsvFile CsvFile::with_options(const CsvOptions &options) const {
  return CsvFile(m_impl->with_options(options));
}

std::shared_ptr<internal::abstract::CsvFile> CsvFile::impl() const {
  return m_impl;
}

MarkdownFile::MarkdownFile(
    std::shared_ptr<internal::abstract::MarkdownFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

Document MarkdownFile::document() const { return Document(m_impl->document()); }

std::shared_ptr<internal::abstract::MarkdownFile> MarkdownFile::impl() const {
  return m_impl;
}

ImageFile::ImageFile(std::shared_ptr<internal::abstract::ImageFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

std::unique_ptr<std::istream> ImageFile::stream() const {
  return m_impl->file()->stream();
}

ArchiveFile::ArchiveFile(std::shared_ptr<internal::abstract::ArchiveFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

Archive ArchiveFile::archive() const { return Archive(m_impl->archive()); }

DocumentFile DocumentFile::from_disk(const std::string &path,
                                     const Logger &logger) {
  return DocumentFile(File::from_disk(path), logger);
}

DocumentFile DocumentFile::from_memory(std::string data, const Logger &logger) {
  return DocumentFile(File::from_memory(std::move(data)), logger);
}

FileType DocumentFile::type(const File &file) {
  return DocumentFile(file).file_type();
}

FileType DocumentFile::type(const std::string &path) {
  return type(File::from_disk(path));
}

FileMeta DocumentFile::meta(const File &file) {
  return DocumentFile(file).file_meta();
}

FileMeta DocumentFile::meta(const std::string &path) {
  return meta(File::from_disk(path));
}

DocumentFile::DocumentFile(
    std::shared_ptr<internal::abstract::DocumentFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

DocumentFile::DocumentFile(const File &file, const Logger &logger)
    : DocumentFile(
          internal::open_strategy::open_document_file(file.impl(), logger)) {}

DocumentFile::DocumentFile(const std::string &path, const Logger &logger)
    : DocumentFile(File::from_disk(path), logger) {}

DocumentType DocumentFile::document_type() const {
  return m_impl->document_type();
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

FontFile::FontFile(std::shared_ptr<internal::abstract::FontFile> impl)
    : DecodedFile(impl), m_impl{std::move(impl)} {}

std::unique_ptr<std::istream> FontFile::stream() const {
  return m_impl->file()->stream();
}

std::shared_ptr<internal::abstract::FontFile> FontFile::impl() const {
  return m_impl;
}

} // namespace odr
