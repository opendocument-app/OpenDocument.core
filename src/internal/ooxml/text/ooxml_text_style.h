#ifndef ODR_INTERNAL_OOXML_TEXT_STYLE_H
#define ODR_INTERNAL_OOXML_TEXT_STYLE_H

#include <internal/common/style.h>
#include <odr/document.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {

class Style final {
public:
  Style(std::string name, pugi::xml_node node, const Style *parent);

  std::string name() const;
  const Style *parent() const;

  const common::ResolvedStyle &resolved() const;

private:
  std::string m_name;
  pugi::xml_node m_node;
  const Style *m_parent;

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
  explicit StyleRegistry(pugi::xml_node styles_root);

  [[nodiscard]] Style *style(const std::string &name) const;

private:
  std::unordered_map<std::string, pugi::xml_node> m_index;

  std::unordered_map<std::string, std::unique_ptr<Style>> m_styles;

  void generate_indices_(pugi::xml_node styles_root);
  void generate_styles_();
  Style *generate_style_(const std::string &name, pugi::xml_node node);
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_STYLE_H
