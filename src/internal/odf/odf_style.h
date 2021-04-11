#ifndef ODR_INTERNAL_ODF_STYLE_H
#define ODR_INTERNAL_ODF_STYLE_H

#include <any>
#include <internal/identifier.h>
#include <memory>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr {
enum class ElementType;
enum class ElementProperty;
} // namespace odr

namespace odr::internal::odf {
class OpenDocument;

class Style {
public:
  Style();
  Style(pugi::xml_node content_root, pugi::xml_node styles_root);

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  resolve_style(ElementType element, const std::string &style_name) const;

private:
  struct Entry {
    std::shared_ptr<Entry> m_parent;
    pugi::xml_node m_node;

    Entry(std::shared_ptr<Entry> parent, pugi::xml_node node);

    [[nodiscard]] std::unordered_map<ElementProperty, std::any>
    properties(ElementType element) const;
  };

  std::unordered_map<std::string, pugi::xml_node> m_index_font_face;
  std::unordered_map<std::string, pugi::xml_node> m_index_default_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_list_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_outline_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_page_layout;
  std::unordered_map<std::string, pugi::xml_node> m_index_master_page;

  std::unordered_map<std::string, std::shared_ptr<Entry>> m_default_styles;
  std::unordered_map<std::string, std::shared_ptr<Entry>> m_styles;

  void generate_indices_(pugi::xml_node content_root,
                         pugi::xml_node styles_root);
  void generate_indices_(pugi::xml_node node);

  void generate_styles_();
  std::shared_ptr<Entry> generate_default_style_(const std::string &name,
                                                 pugi::xml_node node);
  std::shared_ptr<Entry> generate_style_(const std::string &name,
                                         pugi::xml_node node);

  [[nodiscard]] std::shared_ptr<Entry> style_(const std::string &name) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_STYLE_H
