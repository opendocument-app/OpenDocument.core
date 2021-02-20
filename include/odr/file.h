#ifndef ODR_FILE_H
#define ODR_FILE_H

#include <cstdint>
#include <memory>
#include <odr/document.h>
#include <optional>
#include <string>
#include <vector>

namespace odr::abstract {
class File;
class DecodedFile;
class ImageFile;
class DocumentFile;
} // namespace odr::abstract

namespace odr {
class ImageFile;
class DocumentFile;

enum class FileType {
  UNKNOWN,

  // https://en.wikipedia.org/wiki/OpenDocument
  OPENDOCUMENT_TEXT,
  OPENDOCUMENT_PRESENTATION,
  OPENDOCUMENT_SPREADSHEET,
  OPENDOCUMENT_GRAPHICS,

  // https://en.wikipedia.org/wiki/Office_Open_XML
  OFFICE_OPEN_XML_DOCUMENT,
  OFFICE_OPEN_XML_PRESENTATION,
  OFFICE_OPEN_XML_WORKBOOK,
  OFFICE_OPEN_XML_ENCRYPTED,

  // https://en.wikipedia.org/wiki/List_of_Microsoft_Office_filename_extensions
  LEGACY_WORD_DOCUMENT,
  LEGACY_POWERPOINT_PRESENTATION,
  LEGACY_EXCEL_WORKSHEETS,

  // https://en.wikipedia.org/wiki/Rich_Text_Format
  RICH_TEXT_FORMAT,

  // https://en.wikipedia.org/wiki/PDF
  PORTABLE_DOCUMENT_FORMAT,

  // https://en.wikipedia.org/wiki/Text_file
  TEXT_FILE,
  // https://en.wikipedia.org/wiki/Comma-separated_values
  COMMA_SEPARATED_VALUES,
  // https://en.wikipedia.org/wiki/Markdown
  MARKDOWN,

  // https://en.wikipedia.org/wiki/Zip_(file_format)
  ZIP,
  // https://en.wikipedia.org/wiki/Compound_File_Binary_Format
  COMPOUND_FILE_BINARY_FORMAT,

  STARVIEW_METAFILE,
};

enum class FileCategory {
  UNKNOWN,
  TEXT,
  IMAGE,
  ARCHIVE,
  DOCUMENT,
};

enum class FileLocation {
  UNKNOWN,
  MEMORY,
  DISC,
  NETWORK,
};

enum class EncryptionState {
  UNKNOWN,
  NOT_ENCRYPTED,
  ENCRYPTED,
  DECRYPTED,
};

struct FileMeta final {
  static FileType type_by_extension(const std::string &extension) noexcept;
  static FileCategory category_by_type(FileType type) noexcept;

  FileType type{FileType::UNKNOWN};
  bool password_encrypted{false};
  std::optional<DocumentMeta> document_meta;

  std::string type_as_string() const noexcept;
};

class File {
public:
  explicit File(std::shared_ptr<abstract::File>);
  explicit File(const std::string &path);

  [[nodiscard]] FileLocation location() const noexcept;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] std::unique_ptr<std::istream> read() const;

protected:
  std::shared_ptr<abstract::File> m_impl;
};

class DecodedFile {
public:
  static std::vector<FileType> types(const std::string &path);
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit DecodedFile(std::shared_ptr<abstract::DecodedFile>);
  explicit DecodedFile(const std::string &path);
  DecodedFile(const std::string &path, FileType as);

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] FileCategory file_category() const noexcept;
  [[nodiscard]] FileMeta file_meta() const noexcept;

  [[nodiscard]] ImageFile image_file() const;
  [[nodiscard]] DocumentFile document_file() const;

protected:
  std::shared_ptr<abstract::DecodedFile> m_impl;
};

class ImageFile : public DecodedFile {
public:
  explicit ImageFile(std::shared_ptr<abstract::ImageFile>);

private:
  std::shared_ptr<abstract::ImageFile> m_impl;
};

class DocumentFile : public DecodedFile {
public:
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit DocumentFile(std::shared_ptr<abstract::DocumentFile>);
  explicit DocumentFile(const std::string &path);

  [[nodiscard]] bool password_encrypted() const;
  [[nodiscard]] EncryptionState encryption_state() const;
  bool decrypt(const std::string &password);

  [[nodiscard]] DocumentType document_type() const;
  [[nodiscard]] DocumentMeta document_meta() const;

  [[nodiscard]] Document document() const;

private:
  std::shared_ptr<abstract::DocumentFile> m_impl;
};

} // namespace odr

#endif // ODR_FILE_H
