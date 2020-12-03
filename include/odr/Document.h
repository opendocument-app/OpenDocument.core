#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <string>
#include <vector>
#include <odr/DocumentElements.h>

namespace odr {
class DocumentFile;

namespace common {
class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Graphics;
}

class TextDocument;
class Presentation;
class Spreadsheet;
class Graphics;

enum class DocumentType {
  UNKNOWN,
  TEXT,
  PRESENTATION,
  SPREADSHEET,
  GRAPHICS,
};

struct DocumentMeta final {
  struct Entry {
    std::string name;
    std::uint32_t rowCount{0};
    std::uint32_t columnCount{0};
    std::string notes;
  };

  DocumentType documentType{DocumentType::UNKNOWN};
  std::uint32_t entryCount{0};
  std::vector<Entry> entries;
};

class Document {
public:
  DocumentType documentType() const noexcept;
  DocumentMeta documentMeta() const noexcept;

  bool editable() const noexcept;
  bool savable(bool encrypted = false) const noexcept;

  TextDocument textDocument() const;
  Presentation presentation() const;
  Spreadsheet spreadsheet() const;
  Graphics graphics() const;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

protected:
  std::shared_ptr<common::Document> m_document;

private:
  explicit Document(std::shared_ptr<common::Document>);

  friend DocumentFile;
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
    ElementSiblingRange content;
  };

  std::uint32_t slideCount() const;
  std::vector<Slide> slides() const;

private:
  std::shared_ptr<common::Presentation> m_presentation;
};

class Spreadsheet final : public Document {
public:
  struct Sheet {
    std::string name;
    std::uint32_t rowCount{0};
    std::uint32_t columnCount{0};
    TableElement table;
  };

  std::uint32_t sheetCount() const;
  std::vector<Sheet> sheets() const;

private:
  std::shared_ptr<common::Presentation> m_spreadsheet;
};

class Graphics final : public Document {
public:
  struct Page {
    std::string name;
    PageProperties pageProperties;
    ElementSiblingRange content;
  };

  std::uint32_t pageCount() const;
  std::vector<Page> pages() const;

private:
  std::shared_ptr<common::Graphics> m_graphics;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
