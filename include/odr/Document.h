#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <optional>
#include <string>
#include <odr/File.h>

namespace odr {

namespace common {
class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Graphics;
}

enum class FileType;
struct FileMeta;
struct Config;
class TextDocument;
class Element;
class ElementSiblingRange;
class TableElement;

class Document : public File {
public:
  static FileType type(const std::string &path);
  static FileMeta meta(const std::string &path);

  explicit Document(const std::string &path);

  using File::fileType;
  using File::fileCategory;
  using File::fileMeta;
  DocumentType documentType() const noexcept;

  bool editable() const noexcept;
  bool savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

protected:
  std::shared_ptr<common::Document> m_document;
};

class PossiblyPasswordEncryptedDocument : public PossiblyPasswordEncryptedFile<Document> {
public:
  explicit PossiblyPasswordEncryptedDocument(const std::string &path);
  PossiblyPasswordEncryptedDocument(File &&);
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

  PageProperties pageProperties() const;

  ElementSiblingRange content() const;

private:
  std::shared_ptr<common::TextDocument> m_text_document;
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

  ElementSiblingRange slideContent(std::uint32_t index) const;

private:
  std::shared_ptr<common::Presentation> m_presentation;
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

  TableElement sheetTable(std::uint32_t index);

private:
  std::shared_ptr<common::Presentation> m_spreadsheet;
};

class Graphics final : public Document {
public:
  std::uint32_t pageCount() const;

  ElementSiblingRange pageContent(std::uint32_t index);

private:
  std::shared_ptr<common::Graphics> m_graphics;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
