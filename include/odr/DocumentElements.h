#ifndef ODR_DOCUMENTELEMENTS_H
#define ODR_DOCUMENTELEMENTS_H

#include <memory>
#include <optional>
#include <string>

namespace odr {
namespace common {
class Element;
class TextElement;
class Paragraph;
class Span;
class Table;
} // namespace common

class Element;
class ElementSiblingRange;
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

struct FontProperties {
  std::string font;
  std::string size;
  std::string weight;
  std::string style;
  std::string color;
};

struct RectangularProperties {
  std::optional<std::string> top;
  std::optional<std::string> bottom;
  std::optional<std::string> left;
  std::optional<std::string> right;
};

struct ContainerProperties {
  std::optional<std::string> width;
  std::optional<std::string> height;

  RectangularProperties margin;
  RectangularProperties padding;
  RectangularProperties border;

  std::string horizontalAlign;
  std::string verticalAlign;
};

struct ParagraphProperties {
  std::string textAlign;
};

struct TextProperties {
  FontProperties font;
  std::string backgroundColor;
};

class Element final {
public:
  Element();
  explicit Element(std::shared_ptr<const common::Element> impl);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;
  explicit operator bool() const;

  Element parent() const;
  Element firstChild() const;
  Element previousSibling() const;
  Element nextSibling() const;

  ElementSiblingRange children() const;

  ElementType type() const;

  Element unknown() const;
  TextElement text() const;
  Element lineBreak() const;
  Element pageBreak() const;
  ParagraphElement paragraph() const;
  SpanElement span() const;
  LinkElement link() const;
  ImageElement image() const;
  ListElement list() const;
  TableElement table() const;

private:
  std::shared_ptr<const common::Element> m_impl;
};

class ElementSiblingIterator final {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = Element;
  using difference_type = std::ptrdiff_t;
  using pointer = Element *;
  using reference = Element &;

  explicit ElementSiblingIterator(Element element);

  ElementSiblingIterator &operator++();
  ElementSiblingIterator operator++(int) &;
  reference operator*();
  pointer operator->();
  bool operator==(const ElementSiblingIterator &rhs) const;
  bool operator!=(const ElementSiblingIterator &rhs) const;

private:
  Element m_element;
};

class ElementSiblingRange final {
public:
  explicit ElementSiblingRange(Element begin);
  ElementSiblingRange(Element begin, Element end);

  ElementSiblingIterator begin() const;
  ElementSiblingIterator end() const;

  Element front() const;

private:
  Element m_begin;
  Element m_end;
};

class TextElement final {
public:
  TextElement();
  explicit TextElement(std::shared_ptr<const common::TextElement> impl);

  explicit operator bool() const;

  std::string string() const;

private:
  std::shared_ptr<const common::TextElement> m_impl;
};

class ParagraphElement final {
public:
  ParagraphElement();
  explicit ParagraphElement(std::shared_ptr<const common::Paragraph> impl);

  explicit operator bool() const;

  ParagraphProperties paragraphProperties() const;
  TextProperties textProperties() const;

private:
  std::shared_ptr<const common::Paragraph> m_impl;
};

class SpanElement final {
public:
  SpanElement();
  explicit SpanElement(std::shared_ptr<const common::Span> impl);

  explicit operator bool() const;

  TextProperties textProperties() const;

private:
  std::shared_ptr<const common::Span> m_impl;
};

class LinkElement final {
public:
  LinkElement();
  explicit LinkElement(std::shared_ptr<const common::Element> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::Element> m_impl;
};

class ImageElement final {
public:
  ImageElement();
  explicit ImageElement(std::shared_ptr<const common::Element> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::Element> m_impl;
};

class ListElement final {
public:
  ListElement();
  explicit ListElement(std::shared_ptr<const common::Element> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::Element> m_impl;
};

class TableElement final {
public:
  TableElement();
  explicit TableElement(std::shared_ptr<const common::Table> impl);

  explicit operator bool() const;

private:
  std::shared_ptr<const common::Table> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENTELEMENTS_H
