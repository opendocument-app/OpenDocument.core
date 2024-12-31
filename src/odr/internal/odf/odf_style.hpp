#pragma once

#include <odr/document.hpp>

#include <odr/internal/common/style.hpp>
#include <odr/internal/odf/odf_element.hpp>

#include <any>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <pugixml.hpp>

namespace odr {
struct PageLayout;
struct GraphicStyle;
struct ParagraphStyle;
struct TableCellStyle;
struct TableColumnStyle;
struct TableRowStyle;
struct TableStyle;
struct TextStyle;
} // namespace odr

namespace odr::internal::odf {
class Document;
class StyleRegistry;

class Style final {
public:
  Style();
  Style(const StyleRegistry *registry, std::string family, pugi::xml_node node);
  Style(const StyleRegistry *registry, std::string name, pugi::xml_node node,
        Style *parent, Style *family);

  [[nodiscard]] std::string name() const;

  [[nodiscard]] const common::ResolvedStyle &resolved() const;

private:
  const StyleRegistry *m_registry{nullptr};

  std::string m_name;
  pugi::xml_node m_node;
  Style *m_parent{nullptr};
  Style *m_family{nullptr};

  common::ResolvedStyle m_resolved;

  void resolve_style_();

  static void resolve_text_style_(const StyleRegistry *registry,
                                  pugi::xml_node node, TextStyle &result);
  static void resolve_paragraph_style_(pugi::xml_node node,
                                       ParagraphStyle &result);
  static void resolve_table_style_(pugi::xml_node node, TableStyle &result);
  static void resolve_table_column_style_(pugi::xml_node node,
                                          TableColumnStyle &result);
  static void resolve_table_row_style_(pugi::xml_node node,
                                       TableRowStyle &result);
  static void resolve_table_cell_style_(pugi::xml_node node,
                                        TableCellStyle &result);
  static void resolve_graphic_style_(pugi::xml_node node, GraphicStyle &result);
};

class StyleRegistry final {
public:
  StyleRegistry();
  StyleRegistry(Document &document, pugi::xml_node content_root,
                pugi::xml_node styles_root);

  [[nodiscard]] Style *style(const char *name) const;

  [[nodiscard]] PageLayout page_layout(const std::string &name) const;

  [[nodiscard]] pugi::xml_node font_face_node(const std::string &name) const;

  [[nodiscard]] MasterPage *master_page(const std::string &name) const;
  [[nodiscard]] MasterPage *first_master_page() const;

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

  std::unordered_map<std::string, MasterPage *> m_master_page_elements;
  MasterPage *m_first_master_page_element{};

  void generate_indices_(pugi::xml_node content_root,
                         pugi::xml_node styles_root);
  void generate_indices_(pugi::xml_node node);

  void generate_styles_();
  Style *generate_default_style_(const std::string &name, pugi::xml_node node);
  Style *generate_style_(const std::string &name, pugi::xml_node node);

  void generate_master_pages_(Document &);
};

} // namespace odr::internal::odf
