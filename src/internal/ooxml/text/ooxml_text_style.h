#ifndef ODR_INTERNAL_OOXML_TEXT_STYLE_H
#define ODR_INTERNAL_OOXML_TEXT_STYLE_H

#include <odr/document.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {
class Style;

class StyleRegistry final {
public:
  StyleRegistry();
  explicit StyleRegistry(pugi::xml_node styles_root);

  [[nodiscard]] abstract::Style *style(const std::string &name) const;

private:
  std::unordered_map<std::string, pugi::xml_node> m_index;

  std::unordered_map<std::string, std::unique_ptr<abstract::Style>> m_styles;

  void generate_indices_(pugi::xml_node styles_root);
  void generate_styles_();
  abstract::Style *generate_style_(const std::string &name,
                                   pugi::xml_node node);
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_STYLE_H
