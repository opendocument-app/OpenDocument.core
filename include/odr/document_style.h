#ifndef ODR_DOCUMENT_STYLE_H
#define ODR_DOCUMENT_STYLE_H

#include <cstdint>
#include <memory>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr {
class TextDocument;
class SlideElement;
class PageElement;
class ElementPropertyValue;

class PageStyle {
public:
  bool operator==(const PageStyle &rhs) const;
  bool operator!=(const PageStyle &rhs) const;
  explicit operator bool() const;

  ElementPropertyValue width() const;
  ElementPropertyValue height() const;

  ElementPropertyValue margin_top() const;
  ElementPropertyValue margin_bottom() const;
  ElementPropertyValue margin_left() const;
  ElementPropertyValue margin_right() const;

private:
  std::shared_ptr<const internal::abstract::Document> m_impl;
  std::uint64_t m_id{0};

  PageStyle();
  PageStyle(std::shared_ptr<const internal::abstract::Document> impl,
            std::uint64_t id);

  friend TextDocument;
  friend SlideElement;
  friend PageElement;
};

} // namespace odr

#endif // ODR_DOCUMENT_STYLE_H
