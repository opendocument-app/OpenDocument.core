#ifndef ODR_GENERIC_GENERICDOCUMENT_H
#define ODR_GENERIC_GENERICDOCUMENT_H

#include <memory>

namespace odr::generic {

class GenericTable;
class GenericTextDocument;
class GenericPresentation;
class GenericSpreadsheet;

class GenericElement {
public:
  enum class Type {
    UNKNOWN,
    TEXT,
    LINE_BREAK,
    PAGE_BREAK,
    PARAGRAPH,
    SPAN,
    LINK,
    IMAGE,
    LIST,
    TABLE,
  };

  virtual ~GenericElement() = default;

  virtual std::shared_ptr<const GenericElement> parent() const = 0;
  virtual std::shared_ptr<const GenericElement> firstChild() const = 0;
  virtual std::shared_ptr<const GenericElement> previousSibling() const = 0;
  virtual std::shared_ptr<const GenericElement> nextSibling() const = 0;

  virtual Type type() const = 0;
  bool isText() const;
  bool isLineBreak() const;
  bool isPageBreak() const;
  bool isParagraph() const;
  bool isSpan() const;
  bool isLink() const;
  bool isImage() const;
  bool isList() const;
  bool isTable() const;
};

class GenericText : public virtual GenericElement {
public:
  Type type() const final;

  virtual std::string text() const = 0;
};

class GenericParagraph : public virtual GenericElement {
public:
  struct Properties {
  };

  Type type() const final;

  virtual Properties properties() const = 0;
};

class GenericTable : public virtual GenericElement {
public:
  Type type() const final;

  virtual std::uint32_t rows() const = 0;
  virtual std::uint32_t columns() const = 0;

  virtual std::shared_ptr<GenericElement>
  firstContentElement(std::uint32_t row, std::uint32_t column) const = 0;
};

class GenericDocument {
public:
  enum class Type {
    NONE,
    UNKNOWN,
    TEXT,
    PRESENTATION,
    SPREADSHEET,
  };

  virtual ~GenericDocument() = default;

  virtual Type type() const = 0;
  bool isText() const;
  bool isPresentation() const;
  bool isSpreadsheet() const;
};

class GenericTextDocument : public virtual GenericDocument {
public:
  Type type() const final;

  virtual std::shared_ptr<const GenericElement> firstContentElement() const = 0;
};

class GenericPresentation : public virtual GenericDocument {
public:
  Type type() const final;

  virtual std::shared_ptr<GenericElement>
  slideContent(std::uint32_t index) const = 0;
};

class GenericSpreadsheet : public virtual GenericDocument {
public:
  Type type() const final;

  virtual std::shared_ptr<GenericElement> table(std::uint32_t index) const = 0;
};

} // namespace odr::generic

#endif // ODR_GENERIC_GENERICDOCUMENT_H
