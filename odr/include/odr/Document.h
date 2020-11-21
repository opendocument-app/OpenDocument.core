#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <string>
#include <odr/File.h>

namespace odr {

namespace common {
class Document;
}

enum class FileType;
struct FileMeta;
struct Config;
class TextDocument;
class Element;
class TableElement;

class Document : public File {
public:
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit Document(const std::string &path);
  Document(const std::string &path, FileType as);
  Document(File &&);
  Document(Document &&) noexcept;
  ~Document();

  using File::fileType;
  using File::fileCategory;
  using File::fileMeta;
  DocumentType documentType() const noexcept;
  bool encrypted() const noexcept;

  bool decrypted() const noexcept;
  bool translatable() const noexcept;
  bool editable() const noexcept;
  bool savable(bool encrypted = false) const noexcept;

  bool decrypt(const std::string &password) const;

  TextDocument textDocument() const;

  void translate(const std::string &path, const Config &config) const;
  void edit(const std::string &diff) const;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

private:
  common::Document &impl() const noexcept;
};

struct PageProperties {
  std::string width;
  std::string height;
  std::string marginTop;
  std::string marginBottom;
  std::string marginLeft;
  std::string marginRight;
  std::string printOrientation;
};

class TextDocument final : public Document {
public:
  explicit TextDocument(const std::string &path);
  TextDocument(const std::string &path, FileType as);
  TextDocument(TextDocument &&) noexcept;
  ~TextDocument();

  PageProperties pageProperties() const;

  Element firstContentElement() const;
};

class Presentation final : public Document {
public:
  struct Slide {
    std::string name;
    std::string notes;
    PageProperties pageProperties;
  };

  std::uint32_t slideCount() const;
  std::vector<Slide> slides() const;

  Element firstSlideContentElement(std::uint32_t index) const;
};

class Spreadsheet final : public Document {
public:
  struct Sheet {
    std::string name;
    std::uint32_t rowCount{0};
    std::uint32_t columnCount{0};
  };

  std::uint32_t sheetCount() const;
  std::vector<Sheet> sheets() const;

  TableElement sheetContent(std::uint32_t index);
};

} // namespace odr

#endif // ODR_DOCUMENT_H
