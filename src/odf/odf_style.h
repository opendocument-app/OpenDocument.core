#ifndef ODR_ODF_STYLE_H
#define ODR_ODF_STYLE_H

#include <memory>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr::common {
class Property;
class PageStyle;
class TextStyle;
class ParagraphStyle;
class TableStyle;
class TableColumnStyle;
class TableCellStyle;
class DrawingStyle;
} // namespace odr::common

namespace odr::odf {

struct ResolvedStyle {
  static std::shared_ptr<common::Property>
  lookup(const std::unordered_map<std::string, std::string> &map,
         const std::string &attribute);

  std::unordered_map<std::string, std::string> paragraphProperties;
  std::unordered_map<std::string, std::string> textProperties;

  std::unordered_map<std::string, std::string> tableProperties;
  std::unordered_map<std::string, std::string> tableColumnProperties;
  std::unordered_map<std::string, std::string> tableRowProperties;
  std::unordered_map<std::string, std::string> tableCellProperties;

  std::unordered_map<std::string, std::string> chartProperties;
  std::unordered_map<std::string, std::string> drawingPageProperties;
  std::unordered_map<std::string, std::string> graphicProperties;

  std::shared_ptr<common::TextStyle> toTextStyle() const;
  std::shared_ptr<common::ParagraphStyle> toParagraphStyle() const;
  std::shared_ptr<common::TableStyle> toTableStyle() const;
  std::shared_ptr<common::TableColumnStyle> toTableColumnStyle() const;
  std::shared_ptr<common::TableCellStyle> toTableCellStyle() const;
  std::shared_ptr<common::DrawingStyle> toDrawingStyle() const;
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
  Styles(pugi::xml_node stylesRoot, pugi::xml_node contentRoot);

  std::shared_ptr<Style> style(const std::string &name) const;

  std::shared_ptr<common::PageStyle> pageStyle(const std::string &name) const;
  std::shared_ptr<common::PageStyle>
  masterPageStyle(const std::string &name) const;
  std::shared_ptr<common::PageStyle> defaultPageStyle() const;

private:
  pugi::xml_node m_stylesRoot;
  pugi::xml_node m_contentRoot;

  std::unordered_map<std::string, pugi::xml_node> m_indexFontFace;
  std::unordered_map<std::string, pugi::xml_node> m_indexDefaultStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexListStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexOutlineStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexPageLayout;
  std::unordered_map<std::string, pugi::xml_node> m_indexMasterPage;

  std::unordered_map<std::string, std::shared_ptr<Style>> m_defaultStyles;
  std::unordered_map<std::string, std::shared_ptr<Style>> m_styles;

  void generateIndices();
  void generateIndices(pugi::xml_node);

  void generateStyles();
  std::shared_ptr<Style> generateDefaultStyle(const std::string &,
                                              pugi::xml_node);
  std::shared_ptr<Style> generateStyle(const std::string &, pugi::xml_node);
};

} // namespace odr::odf

#endif // ODR_ODF_STYLE_H
