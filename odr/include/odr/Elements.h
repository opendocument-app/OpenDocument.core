#ifndef ODR_ELEMENTS_H
#define ODR_ELEMENTS_H

#include <memory>

namespace odr {

class TextElement;
class ParagraphElement;
class SpanElement;
class LinkElement;
class ImageElement;
class ListElement;
class TableElement;

class Element final {
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

  Element parent() const;
  Element firstChild() const;
  Element previousSibling() const;
  Element nextSibling() const;

  Type type() const;

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
  std::shared_ptr<void> m_impl;
};

class TextElement final {
public:
  const char *string() const;

private:
  std::shared_ptr<void> m_impl;
};

class ParagraphElement final {
public:
private:
  std::shared_ptr<void> m_impl;
};

class SpanElement final {
public:
private:
  std::shared_ptr<void> m_impl;
};

class LinkElement final {
public:
private:
  std::shared_ptr<void> m_impl;
};

class ImageElement final {
public:
private:
  std::shared_ptr<void> m_impl;
};

class ListElement final {
public:
private:
  std::shared_ptr<void> m_impl;
};

class TableElement final {
public:
private:
  std::shared_ptr<void> m_impl;
};

}

#endif // ODR_ELEMENTS_H
