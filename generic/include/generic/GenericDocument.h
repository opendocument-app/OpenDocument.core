#ifndef ODR_GENERIC_GENERICDOCUMENT_H
#define ODR_GENERIC_GENERICDOCUMENT_H

#include <memory>

namespace odr::generic {

class GenericElement {
public:
  enum class Type {
    NONE,
    TEXT,
    LINE_BREAK,
    PAGE_BREAK,
    PARAGRAPH,
    SPAN,
    IMAGE,
    LIST,
    TABLE,
  };

  virtual ~GenericElement() = default;

  virtual std::shared_ptr<GenericElement> root() = 0;

  virtual std::shared_ptr<GenericElement> parent() = 0;
  virtual std::shared_ptr<GenericElement> firstChild() = 0;

  virtual std::shared_ptr<GenericElement> previousSibling() = 0;
  virtual std::shared_ptr<GenericElement> nextSibling() = 0;
};

class GenericTable : public GenericElement {
public:
};

class GenericDocument {
public:
  virtual ~GenericDocument() = default;
};

class GenericTextDocument : public GenericDocument {
public:
  virtual std::shared_ptr<GenericElement> firstContentElement() = 0;
};

class GenericPresentation : public GenericDocument {
public:
  virtual std::shared_ptr<GenericElement> slideContent(std::uint32_t index) = 0;
};

class GenericSpreadsheet : public GenericDocument {
public:
  virtual std::shared_ptr<GenericElement> table(std::uint32_t index) = 0;
};

}

#endif // ODR_GENERIC_GENERICDOCUMENT_H
