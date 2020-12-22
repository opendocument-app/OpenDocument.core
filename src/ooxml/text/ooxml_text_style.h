#ifndef ODR_OOXML_TEXT_STYLE_H
#define ODR_OOXML_TEXT_STYLE_H

#include <memory>
#include <pugixml.hpp>

namespace odr::common {
class Property;
class PageStyle;
} // namespace odr::common

namespace odr::ooxml::text {

class Styles final {
public:
  Styles() = default;
  Styles(pugi::xml_node stylesRoot, pugi::xml_node documentRoot);

  [[nodiscard]] std::shared_ptr<common::PageStyle> pageStyle() const;

private:
  pugi::xml_node m_stylesRoot;
  pugi::xml_node m_documentRoot;
};

} // namespace odr::ooxml::text

#endif // ODR_OOXML_TEXT_STYLE_H
