#include <abstract/document_style.h>
#include <common/property.h>
#include <ooxml/text/ooxml_text_style.h>
#include <util/string_util.h>

namespace odr::ooxml::text {

namespace {
void attributesToMap(pugi::xml_node node,
                     std::unordered_map<std::string, pugi::xml_node> &map) {
  for (auto &&c : node.children()) {
    map[c.name()] = c;
  }
}

class TextPageStyle final : public abstract::PageStyle {
public:
  explicit TextPageStyle(pugi::xml_node node) : m_node{node} {}

  std::shared_ptr<abstract::Property> width() const final {
    return std::make_shared<common::ConstProperty>("8.5in");
  }

  std::shared_ptr<abstract::Property> height() const final {
    return std::make_shared<common::ConstProperty>("11.7in");
  }

  std::shared_ptr<abstract::Property> marginTop() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<abstract::Property> marginBottom() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<abstract::Property> marginLeft() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<abstract::Property> marginRight() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<abstract::Property> printOrientation() const final {
    return std::make_shared<common::ConstProperty>("");
  }

private:
  pugi::xml_node m_node;
};

class ParagraphStyle final : public abstract::ParagraphStyle {
public:
  explicit ParagraphStyle(
      std::unordered_map<std::string, pugi::xml_node> paragraphProperties)
      : m_paragraphProperties{std::move(paragraphProperties)} {}

  std::shared_ptr<abstract::Property> textAlign() const final {
    auto it = m_paragraphProperties.find("w:jc");
    if (it == m_paragraphProperties.end())
      return {};
    std::string alignment = it->second.attribute("w:val").value();
    if (alignment.empty())
      return {};
    if (alignment == "both")
      alignment = "justify";
    return std::make_shared<common::ConstProperty>(alignment);
  }

  std::shared_ptr<abstract::Property> marginTop() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<abstract::Property> marginBottom() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<abstract::Property> marginLeft() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<abstract::Property> marginRight() const final {
    return std::make_shared<common::ConstProperty>();
  }

private:
  std::unordered_map<std::string, pugi::xml_node> m_paragraphProperties;
};

class TextStyle final : public abstract::TextStyle {
public:
  explicit TextStyle(
      std::unordered_map<std::string, pugi::xml_node> textProperties)
      : m_textProperties{std::move(textProperties)} {}

  std::shared_ptr<abstract::Property> fontName() const final {
    auto it = m_textProperties.find("w:rFonts");
    if (it == m_textProperties.end())
      return {};
    std::string fontName = it->second.attribute("w:ascii").value();
    if (fontName.empty())
      return {};
    return std::make_shared<common::ConstProperty>(fontName);
  }

  std::shared_ptr<abstract::Property> fontSize() const final {
    auto it = m_textProperties.find("w:sz");
    if (it == m_textProperties.end())
      return {};
    auto fontSize = it->second.attribute("w:val").as_float(0);
    if (fontSize == 0)
      return {};
    return std::make_shared<common::ConstProperty>(
        util::string::to_string(fontSize * 0.5, 1) + "pt");
  }

  std::shared_ptr<abstract::Property> fontWeight() const final {
    auto it = m_textProperties.find("w:b");
    if (it == m_textProperties.end())
      return {};
    return std::make_shared<common::ConstProperty>("bold");
  }

  std::shared_ptr<abstract::Property> fontStyle() const final {
    auto it = m_textProperties.find("w:i");
    if (it == m_textProperties.end())
      return {};
    return std::make_shared<common::ConstProperty>("italic");
  }

  std::shared_ptr<abstract::Property> fontColor() const final {
    auto it = m_textProperties.find("w:color");
    if (it == m_textProperties.end())
      return {};
    std::string fontColor = it->second.attribute("w:val").value();
    if (fontColor.empty() || (fontColor == "auto"))
      return {};
    if (fontColor.size() == 6)
      fontColor = "#" + fontColor;
    return std::make_shared<common::ConstProperty>(fontColor);
  }

  std::shared_ptr<abstract::Property> backgroundColor() const final {
    auto it = m_textProperties.find("w:highlight");
    if (it == m_textProperties.end())
      return {};
    std::string fontColor = it->second.attribute("w:val").value();
    if (fontColor.empty() || (fontColor == "auto"))
      return {};
    if (fontColor.size() == 6)
      fontColor = "#" + fontColor;
    return std::make_shared<common::ConstProperty>(fontColor);
  }

private:
  std::unordered_map<std::string, pugi::xml_node> m_textProperties;
};
} // namespace

std::shared_ptr<abstract::ParagraphStyle>
ResolvedStyle::toParagraphStyle() const {
  return std::make_shared<ParagraphStyle>(paragraphProperties);
}

std::shared_ptr<abstract::TextStyle> ResolvedStyle::toTextStyle() const {
  return std::make_shared<TextStyle>(textProperties);
}

Style::Style() = default;

Style::Style(std::shared_ptr<Style> parent, pugi::xml_node styleNode)
    : m_parent{std::move(parent)}, m_styleNode{styleNode} {}

ResolvedStyle Style::resolve() const {
  ResolvedStyle result;

  if (m_parent)
    result = m_parent->resolve();

  attributesToMap(m_styleNode.child("w:pPr"), result.paragraphProperties);
  attributesToMap(m_styleNode.child("w:rPr"), result.textProperties);

  return result;
}

ResolvedStyle Style::resolve(pugi::xml_node node) const {
  ResolvedStyle result = resolve();

  attributesToMap(node.child("w:pPr"), result.paragraphProperties);
  attributesToMap(node.child("w:pPr").child("w:rPr"), result.textProperties);
  attributesToMap(node.child("w:rPr"), result.textProperties);

  return result;
}

Styles::Styles(pugi::xml_node stylesRoot, pugi::xml_node documentRoot)
    : m_stylesRoot{stylesRoot}, m_documentRoot{documentRoot} {
  generateIndices();
  generateStyles();
}

void Styles::generateIndices() {
  for (auto style : m_stylesRoot.select_nodes("//w:style")) {
    std::string id = style.node().attribute("w:styleId").value();
    m_indexStyle[id] = style.node();
  }
}

void Styles::generateStyles() {
  for (auto &&e : m_indexStyle) {
    generateStyle(e.first, e.second);
  }
}

std::shared_ptr<Style> Styles::generateStyle(const std::string &name,
                                             pugi::xml_node node) {
  if (auto styleIt = m_styles.find(name); styleIt != m_styles.end())
    return styleIt->second;

  std::shared_ptr<Style> parent;

  if (auto parentAttr = node.child("w:basedOn").attribute("w:val");
      parentAttr) {
    if (auto parentStyleIt = m_indexStyle.find(parentAttr.value());
        parentStyleIt != m_indexStyle.end())
      parent = generateStyle(parentAttr.value(), parentStyleIt->second);
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Style>(parent, node);
}

std::shared_ptr<Style> Styles::style(const std::string &name) const {
  auto styleIt = m_styles.find(name);
  if (styleIt == m_styles.end())
    return {};
  return styleIt->second;
}

std::shared_ptr<abstract::PageStyle> Styles::page_style() const {
  return std::make_shared<TextPageStyle>(
      m_documentRoot.child("w:body").child("w:sectPr"));
}

} // namespace odr::ooxml::text
