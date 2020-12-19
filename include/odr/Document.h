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
class Drawing;
} // namespace common

class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;

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
  Drawing drawing() const;

  Element root() const;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

protected:
  std::shared_ptr<common::Document> m_document;

  explicit Document(std::shared_ptr<common::Document>);

private:
  friend DocumentFile;
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

class Presentation final : public Document {
public:
  std::uint32_t slideCount() const;

  SlideRange slides() const;

private:
  std::shared_ptr<common::Presentation> m_presentation;

  explicit Presentation(std::shared_ptr<common::Presentation>);

  friend Document;
};

class Spreadsheet final : public Document {
public:
  std::uint32_t sheetCount() const;

  SheetRange sheets() const;

private:
  std::shared_ptr<common::Spreadsheet> m_spreadsheet;

  explicit Spreadsheet(std::shared_ptr<common::Spreadsheet>);

  friend Document;
};

class Drawing final : public Document {
public:
  std::uint32_t pageCount() const;

  PageRange pages() const;

private:
  std::shared_ptr<common::Drawing> m_drawing;

  explicit Drawing(std::shared_ptr<common::Drawing>);

  friend Document;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
