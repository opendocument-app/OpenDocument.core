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
class Table;
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

};

class Element final {
public:
  explicit Element(std::shared_ptr<const common::Element> impl);

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
  std::shared_ptr<const common::Element> m_impl;
};

class TextElement final {
public:
  explicit TextElement(std::shared_ptr<const common::TextElement> impl);

  std::string string() const;

private:
  std::shared_ptr<const common::TextElement> m_impl;
};

class ParagraphElement final {
public:
  explicit ParagraphElement(
      std::shared_ptr<const common::Paragraph> impl);

private:
  std::shared_ptr<const common::Paragraph> m_impl;
};

class SpanElement final {
public:
  explicit SpanElement(std::shared_ptr<const common::Element> impl);

private:
  std::shared_ptr<const common::Element> m_impl;
};

class LinkElement final {
public:
  explicit LinkElement(std::shared_ptr<const common::Element> impl);

private:
  std::shared_ptr<const common::Element> m_impl;
};

class ImageElement final {
public:
  explicit ImageElement(std::shared_ptr<const common::Element> impl);

private:
  std::shared_ptr<const common::Element> m_impl;
};

class ListElement final {
public:
  explicit ListElement(std::shared_ptr<const common::Element> impl);

private:
  std::shared_ptr<const common::Element> m_impl;
};

class TableElement final {
public:
  explicit TableElement(std::shared_ptr<const common::Table> impl);

private:
  std::shared_ptr<const common::Table> m_impl;
};

} // namespace odr

#endif // ODR_DOCUMENTELEMENTS_H
