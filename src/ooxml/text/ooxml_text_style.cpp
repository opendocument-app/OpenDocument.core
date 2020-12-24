#include <common/document_style.h>
#include <common/property.h>
#include <ooxml/text/ooxml_text_style.h>

namespace odr::ooxml::text {

namespace {
void attributesToMap(pugi::xml_node node,
                     std::unordered_map<std::string, std::string> &map) {
  for (auto &&c : node.children()) {
    map[c.name()] = c.attribute("w:val").value();
  }
}

class TextPageStyle final : public common::PageStyle {
public:
  explicit TextPageStyle(pugi::xml_node node) : m_node{node} {}

  std::shared_ptr<common::Property> width() const final {
    return std::make_shared<common::ConstProperty>("8.5in");
  }

  std::shared_ptr<common::Property> height() const final {
    return std::make_shared<common::ConstProperty>("11.7in");
  }

  std::shared_ptr<common::Property> marginTop() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> marginBottom() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> marginLeft() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> marginRight() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  std::shared_ptr<common::Property> printOrientation() const final {
    return std::make_shared<common::ConstProperty>("");
  }

private:
  pugi::xml_node m_node;
};

class ParagraphStyle final : public common::ParagraphStyle {
public:
  explicit ParagraphStyle(pugi::xml_node properties)
      : m_properties{properties} {}

  std::shared_ptr<common::Property> textAlign() const final {
    std::string alignment =
        m_properties.child("w:jc").attribute("w:val").value();
    if (alignment.empty())
      return {};
    if (alignment == "both")
      alignment = "justify";
    return std::make_shared<common::ConstProperty>(alignment);
  }

  std::shared_ptr<common::Property> marginTop() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> marginBottom() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> marginLeft() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> marginRight() const final {
    return std::make_shared<common::ConstProperty>();
  }

private:
  pugi::xml_node m_properties;
};

class TextStyle final : public common::TextStyle {
public:
  std::shared_ptr<common::Property> fontName() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontSize() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontWeight() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontStyle() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> fontColor() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<common::Property> backgroundColor() const final {
    return std::make_shared<common::ConstProperty>();
  }
};
} // namespace

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

std::shared_ptr<common::PageStyle> Styles::pageStyle() const {
  return std::make_shared<TextPageStyle>(
      m_documentRoot.child("w:body").child("w:sectPr"));
}

std::shared_ptr<common::ParagraphStyle>
Styles::paragraphStyle(pugi::xml_node node) const {
  auto style = std::make_shared<Style>();
  std::string parent = node.child("w:pStyle").attribute("w:val").value();
  if (auto styleIt = m_styles.find(parent); styleIt != m_styles.end())
    style = styleIt->second;
  return style->resolve(node).toParagraphStyle();
}

std::shared_ptr<common::TextStyle> Styles::textStyle() const {
  return std::make_shared<TextStyle>();
}

} // namespace odr::ooxml::text
