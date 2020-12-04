#ifndef ODR_ODF_STYLE_H
#define ODR_ODF_STYLE_H

#include <pugixml.hpp>
#include <unordered_map>

namespace odr {
struct PageProperties;
}

namespace odr::odf {

class Style {
public:
  Style() = default;
  Style(pugi::xml_document styles, pugi::xml_node contentRoot);

  PageProperties defaultPageProperties() const;
  PageProperties pageProperties(const std::string &name) const;

private:
  pugi::xml_document m_styles;
  pugi::xml_node m_contentRoot;

  std::unordered_map<std::string, pugi::xml_node> m_indexFontFace;
  std::unordered_map<std::string, pugi::xml_node> m_indexDefaultStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexListStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexOutlineStyle;
  std::unordered_map<std::string, pugi::xml_node> m_indexPageLayout;
  std::unordered_map<std::string, pugi::xml_node> m_indexMasterPage;

  void generateIndices();
  void generateIndices(pugi::xml_node);
};

} // namespace odr::odf

#endif // ODR_ODF_STYLE_H
