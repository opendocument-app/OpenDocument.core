#include <common/document_style.h>
#include <common/property.h>
#include <common/xml_properties.h>
#include <cstring>
#include <odf/odf_style.h>
#include <odr/document.h>

namespace odr::odf {

namespace {
void attributesToMap(pugi::xml_node node,
                     std::unordered_map<std::string, std::string> &map) {
  for (auto &&a : node.attributes()) {
    map[a.name()] = a.value();
  }
}

class PageStyle final : public common::PageStyle {
public:
  explicit PageStyle(pugi::xml_node pageLayoutProp)
      : m_pageLayoutProp{pageLayoutProp} {}

  std::shared_ptr<common::Property> width() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_pageLayoutProp.attribute("fo:page-width"));
  }

  std::shared_ptr<common::Property> height() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_pageLayoutProp.attribute("fo:page-height"));
  }

  std::shared_ptr<common::Property> marginTop() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_pageLayoutProp.attribute("fo:margin-top"));
  }

  std::shared_ptr<common::Property> marginBottom() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_pageLayoutProp.attribute("fo:margin-bottom"));
  }

  std::shared_ptr<common::Property> marginLeft() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_pageLayoutProp.attribute("fo:margin-left"));
  }

  std::shared_ptr<common::Property> marginRight() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_pageLayoutProp.attribute("fo:margin-right"));
  }

  std::shared_ptr<common::Property> printOrientation() const final {
    return std::make_shared<common::XmlAttributeProperty>(
        m_pageLayoutProp.attribute("style:print-orientation"));
  }

private:
  pugi::xml_node m_pageLayoutProp;
};

class TextStyle final : public common::TextStyle {
public:
  explicit TextStyle(
      std::unordered_map<std::string, std::string> textProperties)
      : m_textProperties{std::move(textProperties)} {}

  std::shared_ptr<common::Property> fontName() const final {
    return ResolvedStyle::lookup(m_textProperties, "style:font-name");
  }

  std::shared_ptr<common::Property> fontSize() const final {
    return ResolvedStyle::lookup(m_textProperties, "fo:font-size");
  }

  std::shared_ptr<common::Property> fontWeight() const final {
    return ResolvedStyle::lookup(m_textProperties, "fo:font-weight");
  }

  std::shared_ptr<common::Property> fontStyle() const final {
    return ResolvedStyle::lookup(m_textProperties, "fo:font-style");
  }

  std::shared_ptr<common::Property> fontColor() const final {
    return ResolvedStyle::lookup(m_textProperties, "fo:color");
  }

  std::shared_ptr<common::Property> backgroundColor() const final {
    return ResolvedStyle::lookup(m_textProperties, "fo:background-color");
  }

private:
  std::unordered_map<std::string, std::string> m_textProperties;
};

class ParagraphStyle final : public common::ParagraphStyle {
public:
  explicit ParagraphStyle(
      std::unordered_map<std::string, std::string> paragraphProperties)
      : m_paragraphProperties{std::move(paragraphProperties)} {}

  std::shared_ptr<common::Property> textAlign() const final {
    return ResolvedStyle::lookup(m_paragraphProperties, "fo:text-align");
  }

  std::shared_ptr<common::Property> marginTop() const final {
    auto result = ResolvedStyle::lookup(m_paragraphProperties, "fo:margin-top");
    if (!result)
      result = ResolvedStyle::lookup(m_paragraphProperties, "fo:margin");
    return result;
  }

  std::shared_ptr<common::Property> marginBottom() const final {
    auto result =
        ResolvedStyle::lookup(m_paragraphProperties, "fo:margin-bottom");
    if (!result)
      result = ResolvedStyle::lookup(m_paragraphProperties, "fo:margin");
    return result;
  }

  std::shared_ptr<common::Property> marginLeft() const final {
    auto result =
        ResolvedStyle::lookup(m_paragraphProperties, "fo:margin-left");
    if (!result)
      result = ResolvedStyle::lookup(m_paragraphProperties, "fo:margin");
    return result;
  }

  std::shared_ptr<common::Property> marginRight() const final {
    auto result =
        ResolvedStyle::lookup(m_paragraphProperties, "fo:margin-right");
    if (!result)
      result = ResolvedStyle::lookup(m_paragraphProperties, "fo:margin");
    return result;
  }

private:
  std::unordered_map<std::string, std::string> m_paragraphProperties;
};

class TableStyle final : public common::TableStyle {
public:
  explicit TableStyle(
      std::unordered_map<std::string, std::string> tableProperties)
      : m_tableProperties{std::move(tableProperties)} {}

  std::shared_ptr<common::Property> width() const final {
    return ResolvedStyle::lookup(m_tableProperties, "style:width");
  }

private:
  std::unordered_map<std::string, std::string> m_tableProperties;
};

class TableColumnStyle final : public common::TableColumnStyle {
public:
  explicit TableColumnStyle(
      std::unordered_map<std::string, std::string> tableColumnProperties)
      : m_tableColumnProperties{std::move(tableColumnProperties)} {}

  std::shared_ptr<common::Property> width() const final {
    return ResolvedStyle::lookup(m_tableColumnProperties, "style:column-width");
  }

private:
  std::unordered_map<std::string, std::string> m_tableColumnProperties;
};

class TableCellStyle final : public common::TableCellStyle {
public:
  explicit TableCellStyle(
      std::unordered_map<std::string, std::string> tableCellProperties)
      : m_tableCellProperties{std::move(tableCellProperties)} {}

  std::shared_ptr<common::Property> paddingTop() const final {
    auto result =
        ResolvedStyle::lookup(m_tableCellProperties, "fo:padding-top");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:padding");
    return result;
  }

  std::shared_ptr<common::Property> paddingBottom() const final {
    auto result =
        ResolvedStyle::lookup(m_tableCellProperties, "fo:padding-bottom");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:padding");
    return result;
  }

  std::shared_ptr<common::Property> paddingLeft() const final {
    auto result =
        ResolvedStyle::lookup(m_tableCellProperties, "fo:padding-left");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:padding");
    return result;
  }

  std::shared_ptr<common::Property> paddingRight() const final {
    auto result =
        ResolvedStyle::lookup(m_tableCellProperties, "fo:padding-right");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:padding");
    return result;
  }

  std::shared_ptr<common::Property> borderTop() const final {
    auto result = ResolvedStyle::lookup(m_tableCellProperties, "fo:border-top");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:border");
    return result;
  }

  std::shared_ptr<common::Property> borderBottom() const final {
    auto result =
        ResolvedStyle::lookup(m_tableCellProperties, "fo:border-bottom");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:border");
    return result;
  }

  std::shared_ptr<common::Property> borderLeft() const final {
    auto result =
        ResolvedStyle::lookup(m_tableCellProperties, "fo:border-left");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:border");
    return result;
  }

  std::shared_ptr<common::Property> borderRight() const final {
    auto result =
        ResolvedStyle::lookup(m_tableCellProperties, "fo:border-right");
    if (!result)
      result = ResolvedStyle::lookup(m_tableCellProperties, "fo:border");
    return result;
  }

private:
  std::unordered_map<std::string, std::string> m_tableCellProperties;
};

class DrawingStyle final : public common::DrawingStyle {
public:
  explicit DrawingStyle(
      std::unordered_map<std::string, std::string> graphicProperties)
      : m_graphicProperties{std::move(graphicProperties)} {}

  std::shared_ptr<common::Property> strokeWidth() const final {
    return ResolvedStyle::lookup(m_graphicProperties, "svg:stroke-width");
  }

  std::shared_ptr<common::Property> strokeColor() const final {
    return ResolvedStyle::lookup(m_graphicProperties, "svg:stroke-color");
  }

  std::shared_ptr<common::Property> fillColor() const final {
    return ResolvedStyle::lookup(m_graphicProperties, "draw:fill-color");
  }

  std::shared_ptr<common::Property> verticalAlign() const final {
    return ResolvedStyle::lookup(m_graphicProperties,
                                 "draw:textarea-vertical-align");
  }

private:
  std::unordered_map<std::string, std::string> m_graphicProperties;
};
} // namespace

std::shared_ptr<common::Property>
ResolvedStyle::lookup(const std::unordered_map<std::string, std::string> &map,
                      const std::string &attribute) {
  auto it = map.find(attribute);
  std::optional<std::string> result;
  if (it != map.end())
    result = it->second;
  return std::make_shared<common::ConstProperty>(result);
}

std::shared_ptr<common::TextStyle> ResolvedStyle::toTextStyle() const {
  return std::make_shared<TextStyle>(textProperties);
}

std::shared_ptr<common::ParagraphStyle>
ResolvedStyle::toParagraphStyle() const {
  return std::make_shared<ParagraphStyle>(paragraphProperties);
}

std::shared_ptr<common::TableStyle> ResolvedStyle::toTableStyle() const {
  return std::make_shared<TableStyle>(paragraphProperties);
}

std::shared_ptr<common::TableColumnStyle>
ResolvedStyle::toTableColumnStyle() const {
  return std::make_shared<TableColumnStyle>(paragraphProperties);
}

std::shared_ptr<common::TableCellStyle>
ResolvedStyle::toTableCellStyle() const {
  return std::make_shared<TableCellStyle>(paragraphProperties);
}

std::shared_ptr<common::DrawingStyle> ResolvedStyle::toDrawingStyle() const {
  return std::make_shared<DrawingStyle>(paragraphProperties);
}

Style::Style(std::shared_ptr<Style> parent, pugi::xml_node node)
    : m_parent{std::move(parent)}, m_node{node} {}

ResolvedStyle Style::resolve() const {
  ResolvedStyle result;

  if (m_parent)
    result = m_parent->resolve();

  // TODO some property nodes have children e.g. <style:paragraph-properties>
  // TODO some properties use relative measures of their parent's properties
  // e.g. fo:font-size

  attributesToMap(m_node.child("style:paragraph-properties"),
                  result.paragraphProperties);
  attributesToMap(m_node.child("style:text-properties"), result.textProperties);

  attributesToMap(m_node.child("style:table-properties"),
                  result.tableProperties);
  attributesToMap(m_node.child("style:table-column-properties"),
                  result.tableColumnProperties);
  attributesToMap(m_node.child("style:table-row-properties"),
                  result.tableRowProperties);
  attributesToMap(m_node.child("style:table-cell-properties"),
                  result.tableCellProperties);

  attributesToMap(m_node.child("style:chart-properties"),
                  result.chartProperties);
  attributesToMap(m_node.child("style:drawing-page-properties"),
                  result.drawingPageProperties);
  attributesToMap(m_node.child("style:graphic-properties"),
                  result.graphicProperties);

  return result;
}

Styles::Styles(pugi::xml_node stylesRoot, pugi::xml_node contentRoot)
    : m_stylesRoot{stylesRoot}, m_contentRoot{contentRoot} {
  generateIndices();
  generateStyles();
}

std::shared_ptr<Style> Styles::style(const std::string &name) const {
  auto styleIt = m_styles.find(name);
  if (styleIt == m_styles.end())
    return {};
  return styleIt->second;
}

std::shared_ptr<common::PageStyle>
Styles::pageStyle(const std::string &name) const {
  auto pageLayoutIt = m_indexPageLayout.find(name);
  if (pageLayoutIt == m_indexPageLayout.end())
    throw 1; // TODO exception or optional
  auto pageLayoutProp =
      pageLayoutIt->second.child("style:page-layout-properties");

  return std::make_shared<PageStyle>(pageLayoutProp);
}

std::shared_ptr<common::PageStyle>
Styles::masterPageStyle(const std::string &name) const {
  auto masterPageIt = m_indexMasterPage.find(name);
  if (masterPageIt == m_indexMasterPage.end())
    throw 1; // TODO exception or optional
  const std::string pageLayoutName =
      masterPageIt->second.attribute("style:page-layout-name").value();
  return pageStyle(pageLayoutName);
}

std::shared_ptr<common::PageStyle> Styles::defaultPageStyle() const {
  const pugi::xml_node masterStyles =
      m_stylesRoot.child("office:master-styles");
  const pugi::xml_node masterStyle = masterStyles.first_child();
  const std::string pageLayoutName =
      masterStyle.attribute("style:page-layout-name").value();
  return pageStyle(pageLayoutName);
}

void Styles::generateIndices() {
  if (auto fontFaceDecls = m_stylesRoot.child("office:font-face-decls");
      fontFaceDecls) {
    generateIndices(fontFaceDecls);
  }

  if (auto styles = m_stylesRoot.child("office:styles"); styles) {
    generateIndices(styles);
  }

  if (auto automaticStyles = m_stylesRoot.child("office:automatic-styles");
      automaticStyles) {
    generateIndices(automaticStyles);
  }

  if (auto masterStyles = m_stylesRoot.child("office:master-styles");
      masterStyles) {
    generateIndices(masterStyles);
  }

  // content styles

  if (auto fontFaceDecls = m_contentRoot.child("office:font-face-decls");
      fontFaceDecls) {
    generateIndices(fontFaceDecls);
  }

  if (auto automaticStyles = m_contentRoot.child("office:automatic-styles");
      automaticStyles) {
    generateIndices(automaticStyles);
  }
}

void Styles::generateIndices(pugi::xml_node node) {
  for (auto &&e : node) {
    if (std::strcmp("style:font-face", e.name()) == 0) {
      m_indexFontFace[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:default-style", e.name()) == 0) {
      m_indexDefaultStyle[e.attribute("style:family").value()] = e;
    } else if (std::strcmp("style:style", e.name()) == 0) {
      m_indexStyle[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:list-style", e.name()) == 0) {
      m_indexListStyle[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:outline-style", e.name()) == 0) {
      m_indexOutlineStyle[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:page-layout", e.name()) == 0) {
      m_indexPageLayout[e.attribute("style:name").value()] = e;
    } else if (std::strcmp("style:master-page", e.name()) == 0) {
      m_indexMasterPage[e.attribute("style:name").value()] = e;
    }
  }
}

void Styles::generateStyles() {
  for (auto &&e : m_indexDefaultStyle) {
    generateDefaultStyle(e.first, e.second);
  }

  for (auto &&e : m_indexStyle) {
    generateStyle(e.first, e.second);
  }
}

std::shared_ptr<Style> Styles::generateDefaultStyle(const std::string &name,
                                                    pugi::xml_node node) {
  if (auto it = m_defaultStyles.find(name); it != m_defaultStyles.end())
    return it->second;

  return m_defaultStyles[name] = std::make_shared<Style>(nullptr, node);
}

std::shared_ptr<Style> Styles::generateStyle(const std::string &name,
                                             pugi::xml_node node) {
  if (auto styleIt = m_styles.find(name); styleIt != m_styles.end())
    return styleIt->second;

  std::shared_ptr<Style> parent;

  if (auto parentAttr = node.attribute("style:parent-style-name"); parentAttr) {
    if (auto parentStyleIt = m_indexStyle.find(parentAttr.value());
        parentStyleIt != m_indexStyle.end())
      parent = generateStyle(parentAttr.value(), parentStyleIt->second);
    // TODO else throw or log?
  } else if (auto familyAttr = node.attribute("style:family-name");
             familyAttr) {
    if (auto familyStyleIt = m_indexDefaultStyle.find(name);
        familyStyleIt != m_indexDefaultStyle.end())
      parent = generateDefaultStyle(parentAttr.value(), familyStyleIt->second);
    // TODO else throw or log?
  }

  return m_styles[name] = std::make_shared<Style>(parent, node);
}

} // namespace odr::odf
