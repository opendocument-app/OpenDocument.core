#ifndef ODR_INTERNAL_OOXML_TEXT_STYLE_H
#define ODR_INTERNAL_OOXML_TEXT_STYLE_H

#include <odr/document.h>
#include <odr/internal/common/style.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {

class Style final {
public:
  explicit Style(pugi::xml_node node);
  Style(std::string name, pugi::xml_node node, const Style *parent);

  [[nodiscard]] std::string name() const;
  [[nodiscard]] const Style *parent() const;

  [[nodiscard]] const common::ResolvedStyle &resolved() const;

private:
  std::string m_name;
  pugi::xml_node m_node;
  const Style *m_parent{nullptr};

  common::ResolvedStyle m_resolved;

  void resolve_style_();
  void resolve_default_style_();
};

class StyleRegistry final {
public:
  StyleRegistry();
  explicit StyleRegistry(pugi::xml_node styles_root);

  [[nodiscard]] Style *default_style() const;
  [[nodiscard]] Style *style(const std::string &name) const;

  common::ResolvedStyle partial_text_style(pugi::xml_node node) const;
  common::ResolvedStyle partial_paragraph_style(pugi::xml_node node) const;
  common::ResolvedStyle partial_table_style(pugi::xml_node node) const;
  common::ResolvedStyle partial_table_row_style(pugi::xml_node node) const;
  common::ResolvedStyle partial_table_cell_style(pugi::xml_node node) const;

private:
  std::unordered_map<std::string, pugi::xml_node> m_index;

  std::unique_ptr<Style> m_default_style;
  std::unordered_map<std::string, std::unique_ptr<Style>> m_styles;

  void generate_indices_(pugi::xml_node styles_root);
  void generate_styles_(pugi::xml_node styles_root);
  Style *generate_style_(const std::string &name, pugi::xml_node node);
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_STYLE_H
