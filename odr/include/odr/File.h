#ifndef ODR_FILE_H
#define ODR_FILE_H

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace odr {

namespace common {
class File;
}

class Document;
class DocumentNoExcept;

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
  // https://en.wikipedia.org/wiki/PDF
  PORTABLE_DOCUMENT_FORMAT,
  // https://en.wikipedia.org/wiki/Text_file
  TEXT_FILE,
  // https://en.wikipedia.org/wiki/Comma-separated_values
  COMMA_SEPARATED_VALUES,
  // https://en.wikipedia.org/wiki/Rich_Text_Format
  RICH_TEXT_FORMAT,
  // https://en.wikipedia.org/wiki/Markdown
  MARKDOWN,
  // https://en.wikipedia.org/wiki/Zip_(file_format)
  ZIP,
  // https://en.wikipedia.org/wiki/Compound_File_Binary_Format
  COMPOUND_FILE_BINARY_FORMAT,
};

struct FileMeta {
  static FileType typeByExtension(const std::string &extension) noexcept;

  struct Entry {
    std::string name;
    std::uint32_t rowCount{0};
    std::uint32_t columnCount{0};
    std::string notes;
  };

  FileType type{FileType::UNKNOWN};
  bool confident{false};
  bool encrypted{false};
  std::uint32_t entryCount{0};
  std::vector<Entry> entries;

  std::string typeAsString() const noexcept;
};

class File {
public:
  static std::vector<FileType> types(const std::string &path);
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit File(const std::string &path);
  File(const std::string &path, FileType as);
  File(File &&) noexcept;
  virtual ~File();

  FileType type() const noexcept;
  const FileMeta &meta() const noexcept;

  Document document() &&;

protected:
  explicit File(std::unique_ptr<common::File>);

  std::unique_ptr<common::File> impl_;
};

}

#endif // ODR_FILE_H
