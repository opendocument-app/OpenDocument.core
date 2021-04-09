#ifndef ODR_INTERNAL_ODF_STYLE_H
#define ODR_INTERNAL_ODF_STYLE_H

#include <any>
#include <memory>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr {
enum class ElementType;
enum class ElementProperty;
} // namespace odr

namespace odr::internal::odf {

struct ResolvedStyle {
  std::unordered_map<std::string, std::any> paragraph_properties;
  std::unordered_map<std::string, std::any> text_properties;

  std::unordered_map<std::string, std::any> table_properties;
  std::unordered_map<std::string, std::any> table_column_properties;
  std::unordered_map<std::string, std::any> table_row_properties;
  std::unordered_map<std::string, std::any> table_cell_properties;

  std::unordered_map<std::string, std::any> chart_properties;
  std::unordered_map<std::string, std::any> drawing_page_properties;
  std::unordered_map<std::string, std::any> graphic_properties;
};

class Style final {
public:
  Style(std::shared_ptr<Style> parent, pugi::xml_node node);

  [[nodiscard]] ResolvedStyle resolve() const;

private:
  std::shared_ptr<Style> m_parent;
  pugi::xml_node m_node;
};

class Styles {
public:
  Styles() = default;
  Styles(pugi::xml_node styles_root, pugi::xml_node content_root);

  [[nodiscard]] std::shared_ptr<Style> style(const std::string &name) const;

private:
  pugi::xml_node m_styles_root;
  pugi::xml_node m_content_root;

  std::unordered_map<std::string, pugi::xml_node> m_index_font_face;
  std::unordered_map<std::string, pugi::xml_node> m_index_default_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_list_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_outline_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_page_layout;
  std::unordered_map<std::string, pugi::xml_node> m_index_master_page;

  std::unordered_map<std::string, std::shared_ptr<Style>> m_default_styles;
  std::unordered_map<std::string, std::shared_ptr<Style>> m_styles;

  void generate_indices();
  void generate_indices(pugi::xml_node);

  void generate_styles();
  std::shared_ptr<Style> generate_default_style(const std::string &,
                                                pugi::xml_node);
  std::shared_ptr<Style> generate_style(const std::string &, pugi::xml_node);
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_STYLE_H
