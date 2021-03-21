#ifndef ODR_INTERNAL_ODF_STYLE_H
#define ODR_INTERNAL_ODF_STYLE_H

#include <memory>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr::internal::odf {

struct ResolvedStyle {
  static std::shared_ptr<abstract::Property>
  lookup(const std::unordered_map<std::string, std::string> &map,
         const std::string &attribute);

  std::shared_ptr<abstract::TextStyle> to_text_style() const;
  std::shared_ptr<abstract::ParagraphStyle> to_paragraph_style() const;
  std::shared_ptr<abstract::TableStyle> to_table_style() const;
  std::shared_ptr<abstract::TableColumnStyle> to_table_column_style() const;
  std::shared_ptr<abstract::TableCellStyle> to_table_cell_style() const;
  std::shared_ptr<abstract::DrawingStyle> to_drawing_style() const;

  std::unordered_map<std::string, std::string> paragraph_properties;
  std::unordered_map<std::string, std::string> text_properties;

  std::unordered_map<std::string, std::string> table_properties;
  std::unordered_map<std::string, std::string> table_column_properties;
  std::unordered_map<std::string, std::string> table_row_properties;
  std::unordered_map<std::string, std::string> table_cell_properties;

  std::unordered_map<std::string, std::string> chart_properties;
  std::unordered_map<std::string, std::string> drawing_page_properties;
  std::unordered_map<std::string, std::string> graphic_properties;
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

  std::shared_ptr<Style> style(const std::string &name) const;

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
