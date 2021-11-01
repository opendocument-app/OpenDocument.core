#ifndef ODR_FILE_H
#define ODR_FILE_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class File;
class DecodedFile;
class TextFile;
class ImageFile;
class DocumentFile;
} // namespace odr::internal::abstract

namespace odr {
class TextFile;
class ImageFile;
class DocumentFile;

class Document;

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

  // https://en.wikipedia.org/wiki/Rich_Text_Format
  rich_text_format,

  // https://en.wikipedia.org/wiki/PDF
  portable_document_format,

  // https://en.wikipedia.org/wiki/Text_file
  text_file,
  // https://en.wikipedia.org/wiki/Comma-separated_values
  comma_separated_values,
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
};

enum class FileCategory {
  unknown,
  text,
  image,
  archive,
  document,
};

enum class FileLocation {
  unknown,
  memory,
  disc,
  network,
};

enum class EncryptionState {
  unknown,
  not_encrypted,
  encrypted,
  decrypted,
};

enum class DocumentType {
  unknown,
  text,
  presentation,
  spreadsheet,
  drawing,
};

struct DocumentMeta final {
  DocumentMeta();
  DocumentMeta(DocumentType document_type,
               std::optional<std::uint32_t> entry_count);

  DocumentType document_type{DocumentType::unknown};
  std::optional<std::uint32_t> entry_count;
};

struct FileMeta final {
  FileMeta();
  FileMeta(FileType type, bool password_encrypted,
           std::optional<DocumentMeta> document_meta);

  FileType type{FileType::unknown};
  bool password_encrypted{false};
  std::optional<DocumentMeta> document_meta;
};

class File final {
public:
  explicit File(std::shared_ptr<internal::abstract::File>);
  explicit File(const std::string &path);

  [[nodiscard]] FileLocation location() const noexcept;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::unique_ptr<std::istream> stream() const;

  // TODO `impl()` might be a bit dirty
  [[nodiscard]] std::shared_ptr<internal::abstract::File> impl() const;

protected:
  std::shared_ptr<internal::abstract::File> m_impl;
};

class DecodedFile {
public:
  static std::vector<FileType> types(const std::string &path);
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit DecodedFile(std::shared_ptr<internal::abstract::DecodedFile>);
  explicit DecodedFile(const std::string &path);
  DecodedFile(const std::string &path, FileType as);

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] FileCategory file_category() const noexcept;
  [[nodiscard]] FileMeta file_meta() const noexcept;

  [[nodiscard]] TextFile text_file() const;
  [[nodiscard]] ImageFile image_file() const;
  [[nodiscard]] DocumentFile document_file() const;

protected:
  std::shared_ptr<internal::abstract::DecodedFile> m_impl;
};

class TextFile final : public DecodedFile {
public:
  explicit TextFile(std::shared_ptr<internal::abstract::TextFile>);

  [[nodiscard]] std::optional<std::string> charset() const;

  [[nodiscard]] std::unique_ptr<std::istream> stream() const;
  [[nodiscard]] std::string text() const;

private:
  std::shared_ptr<internal::abstract::TextFile> m_impl;
};

class ImageFile final : public DecodedFile {
public:
  explicit ImageFile(std::shared_ptr<internal::abstract::ImageFile>);

  [[nodiscard]] std::unique_ptr<std::istream> stream() const;

private:
  std::shared_ptr<internal::abstract::ImageFile> m_impl;
};

class DocumentFile final : public DecodedFile {
public:
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit DocumentFile(std::shared_ptr<internal::abstract::DocumentFile>);
  explicit DocumentFile(const std::string &path);

  [[nodiscard]] bool password_encrypted() const;
  [[nodiscard]] EncryptionState encryption_state() const;
  bool decrypt(const std::string &password);

  [[nodiscard]] DocumentType document_type() const;
  [[nodiscard]] DocumentMeta document_meta() const;

  [[nodiscard]] Document document() const;

private:
  std::shared_ptr<internal::abstract::DocumentFile> m_impl;
};

} // namespace odr

#endif // ODR_FILE_H
