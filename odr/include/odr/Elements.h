#ifndef ODR_ELEMENTS_H
#define ODR_ELEMENTS_H

#include <memory>
#include <optional>

namespace odr {
namespace common {
class AbstractElement;
class AbstractText;
class AbstractParagraph;
class AbstractTable;
} // namespace common

class TextElement;
class ParagraphElement;
class SpanElement;
class LinkElement;
class ImageElement;
class ListElement;
class TableElement;

enum class ElementType {
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

class Element final {
public:
  explicit Element(std::shared_ptr<const common::AbstractElement> impl);

  std::optional<Element> parent() const;
  std::optional<Element> firstChild() const;
  std::optional<Element> previousSibling() const;
  std::optional<Element> nextSibling() const;

  ElementType type() const;

  std::optional<Element> unknown() const;
  std::optional<TextElement> text() const;
  std::optional<Element> lineBreak() const;
  std::optional<Element> pageBreak() const;
  std::optional<ParagraphElement> paragraph() const;
  std::optional<SpanElement> span() const;
  std::optional<LinkElement> link() const;
  std::optional<ImageElement> image() const;
  std::optional<ListElement> list() const;
  std::optional<TableElement> table() const;

private:
  std::shared_ptr<const common::AbstractElement> m_impl;
};

class TextElement final {
public:
  explicit TextElement(std::shared_ptr<const common::AbstractText> impl);

  std::string string() const;

private:
  std::shared_ptr<const common::AbstractText> m_impl;
};

class ParagraphElement final {
public:
  explicit ParagraphElement(
      std::shared_ptr<const common::AbstractParagraph> impl);

private:
  std::shared_ptr<const common::AbstractParagraph> m_impl;
};

class SpanElement final {
public:
  explicit SpanElement(std::shared_ptr<const common::AbstractElement> impl);

private:
  std::shared_ptr<const common::AbstractElement> m_impl;
};

class LinkElement final {
public:
  explicit LinkElement(std::shared_ptr<const common::AbstractElement> impl);

private:
  std::shared_ptr<const common::AbstractElement> m_impl;
};

class ImageElement final {
public:
  explicit ImageElement(std::shared_ptr<const common::AbstractElement> impl);

private:
  std::shared_ptr<const common::AbstractElement> m_impl;
};

class ListElement final {
public:
  explicit ListElement(std::shared_ptr<const common::AbstractElement> impl);

private:
  std::shared_ptr<const common::AbstractElement> m_impl;
};

class TableElement final {
public:
  explicit TableElement(std::shared_ptr<const common::AbstractTable> impl);

private:
  std::shared_ptr<const common::AbstractTable> m_impl;
};

} // namespace odr

#endif // ODR_ELEMENTS_H
