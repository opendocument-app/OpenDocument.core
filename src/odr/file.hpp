#pragma once

#include <odr/logger.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::abstract {
class File;
class DecodedFile;
class TextFile;
class ImageFile;
class ArchiveFile;
class DocumentFile;
class PdfFile;
class FontFile;
} // namespace odr::internal::abstract

namespace odr {
class TextFile;
class ImageFile;
class ArchiveFile;
class DocumentFile;
class PdfFile;
class FontFile;

class Archive;
class Document;

/// @brief Collection of file types.
enum class FileType {
  unknown,

  // https://en.wikipedia.org/wiki/OpenDocument
  opendocument_text,
  opendocument_presentation,
  opendocument_spreadsheet,
  opendocument_graphics,

  // https://en.wikipedia.org/wiki/Office_Open_XML
  office_open_xml_document,
  office_open_xml_presentation,
  office_open_xml_workbook,
  office_open_xml_encrypted,

  // https://en.wikipedia.org/wiki/List_of_Microsoft_Office_filename_extensions
  legacy_word_document,
  legacy_powerpoint_presentation,
  legacy_excel_worksheets,

  // Detection only — magic recognises these so a caller can report the type,
  // but there is no decoder behind them and opening one throws.
  // https://en.wikipedia.org/wiki/WordPerfect
  word_perfect,
  // https://en.wikipedia.org/wiki/Rich_Text_Format
  rich_text_format,

  // https://en.wikipedia.org/wiki/PDF
  portable_document_format,

  // https://en.wikipedia.org/wiki/Text_file
  text_file,
  // https://en.wikipedia.org/wiki/Comma-separated_values
  comma_separated_values,
  // https://en.wikipedia.org/wiki/JSON
  javascript_object_notation,
  // https://en.wikipedia.org/wiki/Markdown
  markdown,

  // https://en.wikipedia.org/wiki/Zip_(file_format)
  zip,
  // https://en.wikipedia.org/wiki/Compound_File_Binary_Format
  compound_file_binary_format,

  portable_network_graphics,
  graphics_interchange_format,
  jpeg,
  bitmap_image_file,

  starview_metafile,

  // https://en.wikipedia.org/wiki/TrueType
  truetype_font,
  // https://en.wikipedia.org/wiki/OpenType
  opentype_font,
};

/// @brief Collection of file categories.
enum class FileCategory {
  unknown,
  text,
  image,
  archive,
  document,
  font,
};

/// @brief Collection of file locations.
enum class FileLocation {
  memory,
  disk,
};

/// @brief Preference for decoding files.
struct DecodePreference final {
  std::optional<FileType> as_file_type;

  std::vector<FileType> file_type_priority;
};

/// @brief Collection of encryption states.
enum class EncryptionState {
  unknown,
  not_encrypted,
  encrypted,
  decrypted,
};

/// @brief Collection of document types.
enum class DocumentType {
  unknown,
  text,
  presentation,
  spreadsheet,
  drawing,
};

/// @brief Meta information about a file.
///
/// The document fields are only meaningful when @ref document_type is set;
/// `document_type == DocumentType::unknown` means the file is not a document.
struct FileMeta final {
  FileType type{FileType::unknown};
  std::string_view mimetype;
  bool password_encrypted{false};

  DocumentType document_type{DocumentType::unknown};
  std::optional<std::uint32_t> entry_count;

  // Document information properties (e.g. a PDF `/Info` dictionary). All
  // optional; a backend leaves unset what it does not carry.
  std::optional<std::string> title;
  std::optional<std::string> author;
  std::optional<std::string> subject;
  std::optional<std::string> keywords;
  std::optional<std::string> creator;
  std::optional<std::string> producer;
  std::optional<std::string> creation_date;
  std::optional<std::string> modification_date;
};

/// @brief Represents a file.
class File final {
public:
  File();
  explicit File(std::shared_ptr<internal::abstract::File>);
  explicit File(const std::string &path);

  [[nodiscard]] FileLocation location() const noexcept;
  [[nodiscard]] std::size_t size() const;

  [[nodiscard]] std::optional<std::string> disk_path() const;
  [[nodiscard]] std::optional<std::string_view> memory_data() const;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const;
  void pipe(std::ostream &out) const;
  void copy(const std::string &path) const;

  // TODO `impl()` might be a bit dirty
  [[nodiscard]] std::shared_ptr<internal::abstract::File> impl() const;

protected:
  std::shared_ptr<internal::abstract::File> m_impl;
};

/// @brief Represents a decoded file.
class DecodedFile {
public:
  [[nodiscard]] static std::vector<FileType>
  list_file_types(const std::string &path,
                  const Logger &logger = Logger::null());
  [[nodiscard]] static std::string_view
  mimetype(const std::string &path, const Logger &logger = Logger::null());

  explicit DecodedFile(std::shared_ptr<internal::abstract::DecodedFile> impl);
  explicit DecodedFile(const File &file, const Logger &logger = Logger::null());
  DecodedFile(const File &file, FileType as,
              const Logger &logger = Logger::null());
  explicit DecodedFile(const std::string &path,
                       const Logger &logger = Logger::null());
  DecodedFile(const std::string &path, FileType as,
              const Logger &logger = Logger::null());
  DecodedFile(const std::string &path, const DecodePreference &preference,
              const Logger &logger = Logger::null());

  [[nodiscard]] File file() const;

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] FileCategory file_category() const noexcept;
  [[nodiscard]] FileMeta file_meta() const noexcept;

  [[nodiscard]] bool password_encrypted() const;
  [[nodiscard]] EncryptionState encryption_state() const;
  [[nodiscard]] DecodedFile decrypt(const std::string &password) const;

  [[nodiscard]] bool is_decodable() const noexcept;

  [[nodiscard]] bool is_text_file() const;
  [[nodiscard]] bool is_image_file() const;
  [[nodiscard]] bool is_archive_file() const;
  [[nodiscard]] bool is_document_file() const;
  [[nodiscard]] bool is_pdf_file() const;
  [[nodiscard]] bool is_font_file() const;

  [[nodiscard]] TextFile as_text_file() const;
  [[nodiscard]] ImageFile as_image_file() const;
  [[nodiscard]] ArchiveFile as_archive_file() const;
  [[nodiscard]] DocumentFile as_document_file() const;
  [[nodiscard]] PdfFile as_pdf_file() const;
  [[nodiscard]] FontFile as_font_file() const;

protected:
  std::shared_ptr<internal::abstract::DecodedFile> m_impl;
};

/// @brief Represents a text file.
class TextFile final : public DecodedFile {
public:
  explicit TextFile(std::shared_ptr<internal::abstract::TextFile>);

  [[nodiscard]] std::optional<std::string> charset() const;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const;
  [[nodiscard]] std::string text() const;

private:
  std::shared_ptr<internal::abstract::TextFile> m_impl;
};

/// @brief Represents an image file.
class ImageFile final : public DecodedFile {
public:
  explicit ImageFile(std::shared_ptr<internal::abstract::ImageFile>);

  [[nodiscard]] std::unique_ptr<std::istream> stream() const;

private:
  std::shared_ptr<internal::abstract::ImageFile> m_impl;
};

/// @brief Represents an archive file.
class ArchiveFile final : public DecodedFile {
public:
  explicit ArchiveFile(std::shared_ptr<internal::abstract::ArchiveFile>);

  [[nodiscard]] Archive archive() const;

private:
  std::shared_ptr<internal::abstract::ArchiveFile> m_impl;
};

/// @brief Represents a document file.
class DocumentFile final : public DecodedFile {
public:
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit DocumentFile(std::shared_ptr<internal::abstract::DocumentFile>);
  explicit DocumentFile(const std::string &path,
                        const Logger &logger = Logger::null());

  [[nodiscard]] DocumentType document_type() const;

  [[nodiscard]] DocumentFile decrypt(const std::string &password) const;

  [[nodiscard]] Document document() const;

  [[nodiscard]] std::shared_ptr<internal::abstract::DocumentFile> impl() const;

private:
  std::shared_ptr<internal::abstract::DocumentFile> m_impl;
};

/// @brief Represents a PDF file.
class PdfFile final : public DecodedFile {
public:
  explicit PdfFile(std::shared_ptr<internal::abstract::PdfFile>);

  [[nodiscard]] PdfFile decrypt(const std::string &password) const;

  [[nodiscard]] std::shared_ptr<internal::abstract::PdfFile> impl() const;

private:
  std::shared_ptr<internal::abstract::PdfFile> m_impl;
};

/// @brief Represents a font file.
class FontFile final : public DecodedFile {
public:
  explicit FontFile(std::shared_ptr<internal::abstract::FontFile>);

  [[nodiscard]] std::unique_ptr<std::istream> stream() const;

  [[nodiscard]] std::shared_ptr<internal::abstract::FontFile> impl() const;

private:
  std::shared_ptr<internal::abstract::FontFile> m_impl;
};

} // namespace odr
