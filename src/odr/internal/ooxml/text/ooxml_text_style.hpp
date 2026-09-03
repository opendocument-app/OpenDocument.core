#pragma once

#include <odr/style.hpp>

#include <odr/internal/common/style.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {

class Style final {
public:
  explicit Style(pugi::xml_node node);
  Style(std::string name, pugi::xml_node node, const Style *parent);

  [[nodiscard]] std::string name() const;
  [[nodiscard]] const Style *parent() const;

  [[nodiscard]] const ResolvedStyle &resolved() const;
  /// `w:contextualSpacing`, which is applied per paragraph and so is not part
  /// of the resolved style.
  [[nodiscard]] bool contextual_spacing() const;

private:
  std::string m_name;
  pugi::xml_node m_node;
  const Style *m_parent{nullptr};

  ResolvedStyle m_resolved;
  bool m_contextual_spacing{false};

  void resolve_style_();
  void resolve_default_style_();
};

class StyleRegistry final {
public:
  StyleRegistry();
  explicit StyleRegistry(pugi::xml_node styles_root);

  [[nodiscard]] Style *default_style() const;
  [[nodiscard]] Style *style(const std::string &name) const;

  [[nodiscard]] ResolvedStyle partial_text_style(pugi::xml_node node) const;
  [[nodiscard]] ResolvedStyle
  partial_paragraph_style(pugi::xml_node node) const;
  [[nodiscard]] ResolvedStyle partial_table_style(pugi::xml_node node) const;
  [[nodiscard]] ResolvedStyle
  partial_table_row_style(pugi::xml_node node) const;
  [[nodiscard]] ResolvedStyle
  partial_table_cell_style(pugi::xml_node node) const;

private:
  std::unordered_map<std::string, pugi::xml_node> m_index;

  std::unique_ptr<Style> m_default_style;
  std::unordered_map<std::string, std::unique_ptr<Style>> m_styles;

  void generate_indices_(pugi::xml_node styles_root);
  void generate_styles_(pugi::xml_node styles_root);
  Style *generate_style_(const std::string &name, pugi::xml_node node);
};

/// Borders of a `w:tc` spanning `rows` rows under `cell_above`: its own
/// `w:tcBorders` over the neighbour's over the table's ([ECMA-376] 17.4.39).
/// Only the edges the cell leads, so a rule between two cells is one line.
DirectionalStyle<std::string> table_cell_border(pugi::xml_node node,
                                                pugi::xml_node cell_above,
                                                const TableStyle &table_style,
                                                std::uint32_t rows);

/// The graphic style a drawing's `wp:anchor`/`wp:inline` states itself;
/// `styles.xml` carries none.
GraphicStyle read_frame_style(pugi::xml_node inner_node);

/// [ECMA-376] 20.4.2.10/11. A page-relative offset is dropped: the frame stays
/// in the text flow, which is not what it would measure against.
std::optional<Measure> read_frame_offset(pugi::xml_node position);

} // namespace odr::internal::ooxml::text
