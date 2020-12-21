#ifndef ODR_ODF_STYLES_H
#define ODR_ODF_STYLES_H

#include <memory>
#include <pugixml.hpp>
#include <unordered_map>
#include <vector>

namespace odr {
struct PageProperties;
struct TextProperties;
} // namespace odr

namespace odr::common {
class Property;
}

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

  TextProperties toTextProperties() const;
};

class Style final {
public:
  Style(std::shared_ptr<Style> parent, pugi::xml_node styleNode);

  [[nodiscard]] ResolvedStyle resolve() const;

private:
  std::shared_ptr<Style> m_parent;
  pugi::xml_node m_styleNode;
};

class Styles {
public:
  Styles() = default;
  Styles(pugi::xml_node stylesRoot, pugi::xml_node contentRoot);

  std::shared_ptr<Style> style(const std::string &name) const;

  PageProperties pageProperties(const std::string &name) const;
  PageProperties masterPageProperties(const std::string &name) const;
  PageProperties defaultPageProperties() const;

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

#endif // ODR_ODF_STYLES_H
