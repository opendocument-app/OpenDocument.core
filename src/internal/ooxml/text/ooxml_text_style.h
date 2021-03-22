#ifndef ODR_INTERNAL_OOXML_TEXT_STYLE_H
#define ODR_INTERNAL_OOXML_TEXT_STYLE_H

#include <memory>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::ooxml::text {

struct ResolvedStyle {
  std::unordered_map<std::string, pugi::xml_node> paragraphProperties;
  std::unordered_map<std::string, pugi::xml_node> textProperties;
};

class Style final {
public:
  Style();
  Style(std::shared_ptr<Style> parent, pugi::xml_node styleNode);

  [[nodiscard]] ResolvedStyle resolve() const;
  [[nodiscard]] ResolvedStyle resolve(pugi::xml_node node) const;

private:
  std::shared_ptr<Style> m_parent;
  pugi::xml_node m_styleNode;
};

class Styles final {
public:
  Styles() = default;
  Styles(pugi::xml_node stylesRoot, pugi::xml_node documentRoot);

  [[nodiscard]] std::shared_ptr<Style> style(const std::string &name) const;

private:
  pugi::xml_node m_stylesRoot;
  pugi::xml_node m_documentRoot;

  std::unordered_map<std::string, pugi::xml_node> m_indexStyle;

  std::unordered_map<std::string, std::shared_ptr<Style>> m_styles;

  void generateIndices();

  void generateStyles();
  std::shared_ptr<Style> generateStyle(const std::string &, pugi::xml_node);
};

} // namespace odr::internal::ooxml::text

#endif // ODR_INTERNAL_OOXML_TEXT_STYLE_H
