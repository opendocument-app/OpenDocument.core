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

namespace odr::internal::abstract {
class Style;
}

namespace odr::internal::odf {
class Document;

class StyleRegistry final {
public:
  StyleRegistry();
  StyleRegistry(pugi::xml_node content_root, pugi::xml_node styles_root);

  [[nodiscard]] abstract::TextStyle *text_style(const std::string &name) const;
  [[nodiscard]] abstract::ParagraphStyle *
  paragraph_style(const std::string &name) const;
  [[nodiscard]] abstract::TableStyle *
  table_style(const std::string &name) const;
  [[nodiscard]] abstract::TableColumnStyle *
  table_column_style(const std::string &name) const;
  [[nodiscard]] abstract::TableRowStyle *
  table_row_style(const std::string &name) const;
  [[nodiscard]] abstract::TableCellStyle *
  table_cell_style(const std::string &name) const;
  [[nodiscard]] abstract::GraphicStyle *
  graphic_style(const std::string &name) const;
  [[nodiscard]] abstract::PageLayout *
  page_layout(const std::string &name) const;

  [[nodiscard]] pugi::xml_node
  master_page_node(const std::string &master_page_name) const;

  [[nodiscard]] std::optional<std::string> first_master_page() const;

  class StyleCollection;

private:
  std::unordered_map<std::string, pugi::xml_node> m_index_font_face;
  std::unordered_map<std::string, pugi::xml_node> m_index_default_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_list_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_outline_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_page_layout;
  std::unordered_map<std::string, pugi::xml_node> m_index_master_page;

  std::optional<std::string> m_first_master_page;

  std::unordered_map<std::string, std::unique_ptr<StyleCollection>>
      m_default_styles;
  std::unordered_map<std::string, std::unique_ptr<StyleCollection>> m_styles;
  std::unordered_map<std::string, std::unique_ptr<abstract::PageLayout>>
      m_page_layouts;

  void generate_indices_(pugi::xml_node content_root,
                         pugi::xml_node styles_root);
  void generate_indices_(pugi::xml_node node);

  void generate_styles_();
  StyleCollection *generate_default_style_(const std::string &name,
                                           pugi::xml_node node);
  StyleCollection *generate_style_(const std::string &name,
                                   pugi::xml_node node);
  void generate_page_layout_(const std::string &name, pugi::xml_node node);

  [[nodiscard]] StyleCollection *style_(const std::string &name) const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_STYLE_H
