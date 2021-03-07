#ifndef ODR_EXPERIMENTAL_DOCUMENT_H
#define ODR_EXPERIMENTAL_DOCUMENT_H

#include <memory>
#include <odr/document_elements.h>
#include <string>
#include <vector>

namespace odr::internal::abstract {
class Document;
class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;
} // namespace odr::internal::abstract

namespace odr::experimental {
enum class DocumentType;
struct DocumentMeta;
class DocumentFile;

class PageStyle;

class TextDocument;
class Presentation;
class Spreadsheet;
class Drawing;

class Document {
public:
  [[nodiscard]] DocumentType document_type() const noexcept;
  [[nodiscard]] DocumentMeta document_meta() const noexcept;

  [[nodiscard]] bool editable() const noexcept;
  [[nodiscard]] bool savable(bool encrypted = false) const noexcept;

  [[nodiscard]] TextDocument text_document() const;
  [[nodiscard]] Presentation presentation() const;
  [[nodiscard]] Spreadsheet spreadsheet() const;
  [[nodiscard]] Drawing drawing() const;

  [[nodiscard]] Element root() const;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

protected:
  std::shared_ptr<internal::abstract::Document> m_document;

  explicit Document(std::shared_ptr<internal::abstract::Document>);

private:
  friend DocumentFile;
};

class TextDocument final : public Document {
public:
  [[nodiscard]] PageStyle page_style() const;

  [[nodiscard]] ElementRange content() const;

private:
  std::shared_ptr<internal::abstract::TextDocument> m_text_document;

  explicit TextDocument(std::shared_ptr<internal::abstract::TextDocument>);

  friend Document;
};

class Presentation final : public Document {
public:
  [[nodiscard]] std::uint32_t slide_count() const;

  [[nodiscard]] SlideRange slides() const;

private:
  std::shared_ptr<internal::abstract::Presentation> m_presentation;

  explicit Presentation(std::shared_ptr<internal::abstract::Presentation>);

  friend Document;
};

class Spreadsheet final : public Document {
public:
  [[nodiscard]] std::uint32_t sheet_count() const;

  [[nodiscard]] SheetRange sheets() const;

private:
  std::shared_ptr<internal::abstract::Spreadsheet> m_spreadsheet;

  explicit Spreadsheet(std::shared_ptr<internal::abstract::Spreadsheet>);

  friend Document;
};

class Drawing final : public Document {
public:
  [[nodiscard]] std::uint32_t page_count() const;

  [[nodiscard]] PageRange pages() const;

private:
  std::shared_ptr<internal::abstract::Drawing> m_drawing;

  explicit Drawing(std::shared_ptr<internal::abstract::Drawing>);

  friend Document;
};

} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_DOCUMENT_H
