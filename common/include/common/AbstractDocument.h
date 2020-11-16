#ifndef ODR_COMMON_ABSTRACTDOCUMENT_H
#define ODR_COMMON_ABSTRACTDOCUMENT_H

#include <memory>

namespace odr::common {

class AbstractTable;
class AbstractTextDocument;
class AbstractPresentation;
class AbstractSpreadsheet;

class AbstractElement {
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

  virtual ~AbstractElement() = default;

  virtual std::shared_ptr<const AbstractElement> parent() const = 0;
  virtual std::shared_ptr<const AbstractElement> firstChild() const = 0;
  virtual std::shared_ptr<const AbstractElement> previousSibling() const = 0;
  virtual std::shared_ptr<const AbstractElement> nextSibling() const = 0;

  virtual Type type() const = 0;
  bool isUnknown() const;
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

class AbstractText : public virtual AbstractElement {
public:
  Type type() const final;

  virtual std::string text() const = 0;
};

class AbstractParagraph : public virtual AbstractElement {
public:
  struct Properties {
  };

  Type type() const final;

  virtual Properties properties() const = 0;
};

class AbstractTable : public virtual AbstractElement {
public:
  Type type() const final;

  virtual std::uint32_t rows() const = 0;
  virtual std::uint32_t columns() const = 0;

  virtual std::shared_ptr<AbstractElement>
  firstContentElement(std::uint32_t row, std::uint32_t column) const = 0;
};

class AbstractDocument {
public:
  enum class Type {
    NONE,
    UNKNOWN,
    TEXT,
    PRESENTATION,
    SPREADSHEET,
  };

  virtual ~AbstractDocument() = default;

  virtual Type type() const = 0;
  bool isText() const;
  bool isPresentation() const;
  bool isSpreadsheet() const;
};

class AbstractTextDocument : public virtual AbstractDocument {
public:
  struct PageProperties {
    std::string width;
    std::string height;
    std::string marginTop;
    std::string marginBottom;
    std::string marginLeft;
    std::string marginRight;
    std::string printOrientation;
  };

  Type type() const final;

  virtual PageProperties pageProperties() const = 0;

  virtual std::shared_ptr<const AbstractElement> firstContentElement() const = 0;
};

class AbstractPresentation : public virtual AbstractDocument {
public:
  Type type() const final;

  virtual std::shared_ptr<AbstractElement>
  slideContent(std::uint32_t index) const = 0;
};

class AbstractSpreadsheet : public virtual AbstractDocument {
public:
  Type type() const final;

  virtual std::shared_ptr<AbstractElement> table(std::uint32_t index) const = 0;
};

} // namespace odr

#endif // ODR_COMMON_ABSTRACTDOCUMENT_H
