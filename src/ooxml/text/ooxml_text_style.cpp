#include <abstract/document_style.h>
#include <common/property.h>
#include <ooxml/text/ooxml_text_style.h>
#include <util/string_util.h>

namespace odr::ooxml::text {

namespace {
void attributes_to_map(pugi::xml_node node,
                       std::unordered_map<std::string, pugi::xml_node> &map) {
  for (auto &&c : node.children()) {
    map[c.name()] = c;
  }
}

class TextPageStyle final : public abstract::PageStyle {
public:
  explicit TextPageStyle(pugi::xml_node node) : m_node{node} {}

  [[nodiscard]] std::shared_ptr<abstract::Property> width() const final {
    return std::make_shared<common::ConstProperty>("8.5in");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> height() const final {
    return std::make_shared<common::ConstProperty>("11.7in");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_top() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  margin_bottom() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_left() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property> margin_right() const final {
    return std::make_shared<common::ConstProperty>("");
  }

  [[nodiscard]] std::shared_ptr<abstract::Property>
  print_orientation() const final {
    return std::make_shared<common::ConstProperty>("");
  }

private:
  pugi::xml_node m_node;
};

class ParagraphStyle final : public abstract::ParagraphStyle {
public:
  explicit ParagraphStyle(
      std::unordered_map<std::string, pugi::xml_node> paragraph_properties)
      : m_paragraph_properties{std::move(paragraph_properties)} {}

  std::shared_ptr<abstract::Property> text_align() const final {
    auto it = m_paragraph_properties.find("w:jc");
    if (it == std::end(m_paragraph_properties)) {
      return {};
    }
    std::string alignment = it->second.attribute("w:val").value();
    if (alignment.empty()) {
      return {};
    }
    if (alignment == "both") {
      alignment = "justify";
    }
    return std::make_shared<common::ConstProperty>(alignment);
  }

  std::shared_ptr<abstract::Property> margin_top() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<abstract::Property> margin_bottom() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<abstract::Property> margin_left() const final {
    return std::make_shared<common::ConstProperty>();
  }

  std::shared_ptr<abstract::Property> margin_right() const final {
    return std::make_shared<common::ConstProperty>();
  }

private:
  std::unordered_map<std::string, pugi::xml_node> m_paragraph_properties;
};

class TextStyle final : public abstract::TextStyle {
public:
  explicit TextStyle(
      std::unordered_map<std::string, pugi::xml_node> text_properties)
      : m_text_properties{std::move(text_properties)} {}

  std::shared_ptr<abstract::Property> font_name() const final {
    auto it = m_text_properties.find("w:rFonts");
    if (it == std::end(m_text_properties)) {
      return {};
    }
    std::string fontName = it->second.attribute("w:ascii").value();
    if (fontName.empty()) {
      return {};
    }
    return std::make_shared<common::ConstProperty>(fontName);
  }

  std::shared_ptr<abstract::Property> font_size() const final {
    auto it = m_text_properties.find("w:sz");
    if (it == std::end(m_text_properties)) {
      return {};
    }
    auto font_size = it->second.attribute("w:val").as_float(0);
    if (font_size == 0) {
      return {};
    }
    return std::make_shared<common::ConstProperty>(
        util::string::to_string(font_size * 0.5, 1) + "pt");
  }

  std::shared_ptr<abstract::Property> font_weight() const final {
    auto it = m_text_properties.find("w:b");
    if (it == std::end(m_text_properties)) {
      return {};
    }
    return std::make_shared<common::ConstProperty>("bold");
  }

  std::shared_ptr<abstract::Property> font_style() const final {
    auto it = m_text_properties.find("w:i");
    if (it == std::end(m_text_properties)) {
      return {};
    }
    return std::make_shared<common::ConstProperty>("italic");
  }

  std::shared_ptr<abstract::Property> font_color() const final {
    auto it = m_text_properties.find("w:color");
    if (it == std::end(m_text_properties)) {
      return {};
    }
    std::string font_color = it->second.attribute("w:val").value();
    if (font_color.empty() || (font_color == "auto")) {
      return {};
    }
    if (font_color.size() == 6) {
      font_color = "#" + font_color;
    }
    return std::make_shared<common::ConstProperty>(font_color);
  }

  std::shared_ptr<abstract::Property> background_color() const final {
    auto it = m_text_properties.find("w:highlight");
    if (it == std::end(m_text_properties)) {
      return {};
    }
    std::string fontColor = it->second.attribute("w:val").value();
    if (fontColor.empty() || (fontColor == "auto")) {
      return {};
    }
    if (fontColor.size() == 6) {
      fontColor = "#" + fontColor;
    }
    return std::make_shared<common::ConstProperty>(fontColor);
  }

private:
  std::unordered_map<std::string, pugi::xml_node> m_text_properties;
};
} // namespace

std::shared_ptr<abstract::ParagraphStyle>
ResolvedStyle::to_paragraph_style() const {
  return std::make_shared<ParagraphStyle>(paragraphProperties);
}

std::shared_ptr<abstract::TextStyle> ResolvedStyle::to_text_style() const {
  return std::make_shared<TextStyle>(textProperties);
}

Style::Style() = default;

Style::Style(std::shared_ptr<Style> parent, pugi::xml_node styleNode)
    : m_parent{std::move(parent)}, m_styleNode{styleNode} {}

ResolvedStyle Style::resolve() const {
  ResolvedStyle result;

  if (m_parent) {
    result = m_parent->resolve();
  }

  attributes_to_map(m_styleNode.child("w:pPr"), result.paragraphProperties);
  attributes_to_map(m_styleNode.child("w:rPr"), result.textProperties);

  return result;
}

ResolvedStyle Style::resolve(pugi::xml_node node) const {
  ResolvedStyle result = resolve();

  attributes_to_map(node.child("w:pPr"), result.paragraphProperties);
  attributes_to_map(node.child("w:pPr").child("w:rPr"), result.textProperties);
  attributes_to_map(node.child("w:rPr"), result.textProperties);

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
  if (auto styleIt = m_styles.find(name); styleIt != std::end(m_styles)) {
    return styleIt->second;
  }

  std::shared_ptr<Style> parent;

  if (auto parentAttr = node.child("w:basedOn").attribute("w:val");
      parentAttr) {
    if (auto parentStyleIt = m_indexStyle.find(parentAttr.value());
        parentStyleIt != std::end(m_indexStyle)) {
      parent = generateStyle(parentAttr.value(), parentStyleIt->second);
    }
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Style>(parent, node);
}

std::shared_ptr<Style> Styles::style(const std::string &name) const {
  auto styleIt = m_styles.find(name);
  if (styleIt == std::end(m_styles)) {
    return {};
  }
  return styleIt->second;
}

std::shared_ptr<abstract::PageStyle> Styles::page_style() const {
  return std::make_shared<TextPageStyle>(
      m_documentRoot.child("w:body").child("w:sectPr"));
}

} // namespace odr::ooxml::text
