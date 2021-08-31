#ifndef ODR_INTERNAL_OOXML_TEXT_STYLE_H
#define ODR_INTERNAL_OOXML_TEXT_STYLE_H

#include <odr/document.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {

class StyleRegistry final {
public:
  StyleRegistry();
  explicit StyleRegistry(pugi::xml_node styles_root);

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  resolve_style(ElementType element_type, pugi::xml_node element) const;

private:
  struct Entry {
    std::shared_ptr<Entry> m_parent;
    pugi::xml_node m_node;

    Entry(std::shared_ptr<Entry> parent, pugi::xml_node node);

    void
    properties(ElementType element,
               std::unordered_map<ElementProperty, std::any> &result) const;
  };

  std::unordered_map<std::string, pugi::xml_node> m_index;

  std::unordered_map<std::string, std::shared_ptr<Entry>> m_styles;

  void generate_indices_(pugi::xml_node styles_root);
  void generate_styles_();
  std::shared_ptr<Entry> generate_style_(const std::string &name,
                                         pugi::xml_node node);
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_STYLE_H
