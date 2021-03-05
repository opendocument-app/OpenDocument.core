#ifndef ODR_DOCUMENT_H
#define ODR_DOCUMENT_H

#include <memory>
#include <odr/document_elements.h>
#include <string>
#include <vector>

namespace odr::abstract {
class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;
} // namespace odr::abstract

namespace odr {
class DocumentFile;

class PageStyle;

class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;

enum class DocumentType {
  UNKNOWN,
  TEXT,
  PRESENTATION,
  SPREADSHEET,
  DRAWING,
};

struct TableDimensions {
  std::uint32_t rows{0};
  std::uint32_t columns{0};

  TableDimensions();
  TableDimensions(std::uint32_t rows, std::uint32_t columns);
};

struct DocumentMeta final {
  struct Entry {
    std::optional<std::string> name;
    std::optional<TableDimensions> table_dimensions;
    std::optional<std::string> notes;

    Entry();
  };

  DocumentMeta();
  DocumentMeta(DocumentType document_type, std::uint32_t entry_count,
               std::vector<Entry> entries);

  DocumentType document_type{DocumentType::UNKNOWN};
  std::uint32_t entry_count{0};
  std::vector<Entry> entries;
};

class Document {
public:
  [[nodiscard]] DocumentType document_type() const noexcept;
  [[nodiscard]] DocumentMeta document_meta() const noexcept;

  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  [[nodiscard]] TextDocument text_tocument() const;
  [[nodiscard]] Presentation presentation() const;
  [[nodiscard]] Spreadsheet spreadsheet() const;
  [[nodiscard]] Drawing drawing() const;

  [[nodiscard]] Element root() const;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

protected:
  std::shared_ptr<abstract::Document> m_document;

  explicit Document(std::shared_ptr<abstract::Document>);

private:
  friend DocumentFile;
};

class TextDocument final : public Document {
public:
  [[nodiscard]] PageStyle page_style() const;

  [[nodiscard]] ElementRange content() const;

private:
  std::shared_ptr<abstract::TextDocument> m_text_document;

  explicit TextDocument(std::shared_ptr<abstract::TextDocument>);

  friend Document;
};

class Presentation final : public Document {
public:
  [[nodiscard]] std::uint32_t slide_count() const;

  [[nodiscard]] SlideRange slides() const;

private:
  std::shared_ptr<abstract::Presentation> m_presentation;

  explicit Presentation(std::shared_ptr<abstract::Presentation>);

  friend Document;
};

class Spreadsheet final : public Document {
public:
  [[nodiscard]] std::uint32_t sheet_count() const;

  [[nodiscard]] SheetRange sheets() const;

private:
  std::shared_ptr<abstract::Spreadsheet> m_spreadsheet;

  explicit Spreadsheet(std::shared_ptr<abstract::Spreadsheet>);

  friend Document;
};

class Drawing final : public Document {
public:
  [[nodiscard]] std::uint32_t page_count() const;

  [[nodiscard]] PageRange pages() const;

private:
  std::shared_ptr<abstract::Drawing> m_drawing;

  explicit Drawing(std::shared_ptr<abstract::Drawing>);

  friend Document;
};

} // namespace odr

#endif // ODR_DOCUMENT_H
