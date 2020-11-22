#ifndef ODR_COMMON_ABSTRACTDOCUMENT_H
#define ODR_COMMON_ABSTRACTDOCUMENT_H

#include <memory>
#include <optional>

namespace odr {
enum class ElementType;
enum class DocumentType;
struct PageProperties;
class Element;

namespace common {

class AbstractTable;
class AbstractTextDocument;
class AbstractPresentation;
class AbstractSpreadsheet;

class AbstractElement {
public:
  std::optional<Element>
  static convert(std::shared_ptr<const common::AbstractElement> element);

  virtual ~AbstractElement() = default;

  virtual std::shared_ptr<const AbstractElement> parent() const = 0;
  virtual std::shared_ptr<const AbstractElement> firstChild() const = 0;
  virtual std::shared_ptr<const AbstractElement> previousSibling() const = 0;
  virtual std::shared_ptr<const AbstractElement> nextSibling() const = 0;

  virtual ElementType type() const = 0;
};

class AbstractText : public virtual AbstractElement {
public:
  ElementType type() const final;

  virtual std::string text() const = 0;
};

class AbstractParagraph : public virtual AbstractElement {
public:
  struct Properties {
  };

  ElementType type() const final;

  virtual Properties properties() const = 0;
};

class AbstractTable : public virtual AbstractElement {
public:
  ElementType type() const final;

  virtual std::uint32_t rows() const = 0;
  virtual std::uint32_t columns() const = 0;

  virtual std::shared_ptr<AbstractElement>
  firstContentElement(std::uint32_t row, std::uint32_t column) const = 0;
};

class AbstractDocument {
public:
  virtual ~AbstractDocument() = default;

  virtual DocumentType type() const = 0;
  bool isText() const;
  bool isPresentation() const;
  bool isSpreadsheet() const;
};

class AbstractTextDocument : public virtual AbstractDocument {
public:
  DocumentType type() const final;

  virtual PageProperties pageProperties() const = 0;

  virtual std::shared_ptr<const AbstractElement> firstContentElement() const = 0;
};

class AbstractPresentation : public virtual AbstractDocument {
public:
  DocumentType type() const final;

  virtual std::shared_ptr<AbstractElement>
  slideContent(std::uint32_t index) const = 0;
};

class AbstractSpreadsheet : public virtual AbstractDocument {
public:
  DocumentType type() const final;

  virtual std::shared_ptr<AbstractElement> table(std::uint32_t index) const = 0;
};

} // namespace odr
}

#endif // ODR_COMMON_ABSTRACTDOCUMENT_H
