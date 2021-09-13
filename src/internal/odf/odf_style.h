#ifndef ODR_INTERNAL_ODF_STYLE_H
#define ODR_INTERNAL_ODF_STYLE_H

#include <any>
#include <internal/common/style.h>
#include <memory>
#include <odr/document.h>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr {
struct PageLayout;
} // namespace odr

namespace odr::internal::odf {
class Document;

class Style final {
public:
  Style();
  Style(std::string family, pugi::xml_node node);
  Style(std::string name, pugi::xml_node node, Style *parent, Style *family);

  [[nodiscard]] std::string name() const;

  [[nodiscard]] const common::ResolvedStyle &resolved() const;

private:
  std::string m_name;
  pugi::xml_node m_node;
  Style *m_parent{nullptr};
  Style *m_family{nullptr};

  common::ResolvedStyle m_resolved;

  void resolve_style_();

  static void resolve_text_style_(pugi::xml_node node,
                                  std::optional<TextStyle> &result);
  static void resolve_paragraph_style_(pugi::xml_node node,
                                       std::optional<ParagraphStyle> &result);
  static void resolve_table_style_(pugi::xml_node node,
                                   std::optional<TableStyle> &result);
  static void
  resolve_table_column_style_(pugi::xml_node node,
                              std::optional<TableColumnStyle> &result);
  static void resolve_table_row_style_(pugi::xml_node node,
                                       std::optional<TableRowStyle> &result);
  static void resolve_table_cell_style_(pugi::xml_node node,
                                        std::optional<TableCellStyle> &result);
  static void resolve_graphic_style_(pugi::xml_node node,
                                     std::optional<GraphicStyle> &result);
};

class StyleRegistry final {
public:
  StyleRegistry();
  StyleRegistry(pugi::xml_node content_root, pugi::xml_node styles_root);

  Style *style(const char *name) const;

  [[nodiscard]] PageLayout page_layout(const std::string &name) const;

  [[nodiscard]] pugi::xml_node
  master_page_node(const std::string &master_page_name) const;

  [[nodiscard]] std::optional<std::string> first_master_page() const;

private:
  std::unordered_map<std::string, pugi::xml_node> m_index_font_face;
  std::unordered_map<std::string, pugi::xml_node> m_index_default_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_list_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_outline_style;
  std::unordered_map<std::string, pugi::xml_node> m_index_page_layout;
  std::unordered_map<std::string, pugi::xml_node> m_index_master_page;

  std::optional<std::string> m_first_master_page;

  std::unordered_map<std::string, std::unique_ptr<Style>> m_default_styles;
  std::unordered_map<std::string, std::unique_ptr<Style>> m_styles;

  void generate_indices_(pugi::xml_node content_root,
                         pugi::xml_node styles_root);
  void generate_indices_(pugi::xml_node node);

  void generate_styles_();
  Style *generate_default_style_(const std::string &name, pugi::xml_node node);
  Style *generate_style_(const std::string &name, pugi::xml_node node);
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_STYLE_H
