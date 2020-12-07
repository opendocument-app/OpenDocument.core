#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <odr/DocumentElements.h>
#include <string>
#include <vector>

namespace odr {
class DocumentFile;

namespace common {
class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Graphics;
} // namespace common

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

  explicit Document(std::shared_ptr<common::Document>);

private:
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

  ElementRange content() const;

private:
  std::shared_ptr<common::TextDocument> m_textDocument;

  explicit TextDocument(std::shared_ptr<common::TextDocument>);

  friend Document;
};

struct Slide {
  std::string name;
  std::string notes;
  PageProperties pageProperties;
  ElementRange content;
};

class Presentation final : public Document {
public:
  std::uint32_t slideCount() const;
  std::vector<Slide> slides() const;

private:
  std::shared_ptr<common::Presentation> m_presentation;

  explicit Presentation(std::shared_ptr<common::Presentation>);

  friend Document;
};

struct Sheet {
  std::string name;
  std::uint32_t rowCount{0};
  std::uint32_t columnCount{0};
  TableElement table;
};

class Spreadsheet final : public Document {
public:
  std::uint32_t sheetCount() const;
  std::vector<Sheet> sheets() const;

private:
  std::shared_ptr<common::Spreadsheet> m_spreadsheet;

  explicit Spreadsheet(std::shared_ptr<common::Spreadsheet>);

  friend Document;
};

struct Page {
  std::string name;
  PageProperties pageProperties;
  ElementRange content;
};

class Graphics final : public Document {
public:
  std::uint32_t pageCount() const;
  std::vector<Page> pages() const;

private:
  std::shared_ptr<common::Graphics> m_graphics;

  explicit Graphics(std::shared_ptr<common::Graphics>);

  friend Document;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
