#include <common/Property.h>
#include <common/XmlProperties.h>
#include <cstring>
#include <odf/Styles.h>
#include <odr/Document.h>

namespace odr::odf {

namespace {
void attributesToMap(pugi::xml_node node,
                     std::unordered_map<std::string, std::string> &map) {
  for (auto &&a : node.attributes()) {
    map[a.name()] = a.value();
  }
}

Property lookup(const std::unordered_map<std::string, std::string> &map,
                const std::string &attribute) {
  auto it = map.find(attribute);
  std::optional<std::string> result;
  if (it != map.end())
    result = it->second;
  return Property(std::make_shared<common::ConstProperty>(result));
}

RectangularProperties
lookupRect(const std::unordered_map<std::string, std::string> &map,
           const std::string &attributePrefix) {
  RectangularProperties result;
  result.top = lookup(map, attributePrefix + "top");
  result.bottom = lookup(map, attributePrefix + "bottom");
  result.left = lookup(map, attributePrefix + "left");
  result.right = lookup(map, attributePrefix + "right");
  return result;
}
} // namespace

ParagraphProperties ResolvedStyle::toParagraphProperties() const {
  ParagraphProperties result;

  result.textAlign = lookup(paragraphProperties, "fo:text-align");
  result.margin = lookupRect(paragraphProperties, "fo:margin-");

  return result;
}

TextProperties ResolvedStyle::toTextProperties() const {
  TextProperties result;

  result.font.font = lookup(textProperties, "style:font-name");
  result.font.size = lookup(textProperties, "fo:font-size");
  result.font.weight = lookup(textProperties, "fo:font-weight");
  result.font.style = lookup(textProperties, "fo:font-style");
  result.font.color = lookup(textProperties, "fo:color");

  result.backgroundColor = lookup(textProperties, "fo:background-color");

  return result;
}

TableProperties ResolvedStyle::toTableProperties() const {
  TableProperties result;

  result.width = lookup(tableProperties, "style:width");

  return result;
}

TableColumnProperties ResolvedStyle::toTableColumnProperties() const {
  TableColumnProperties result;

  result.width = lookup(tableColumnProperties, "style:column-width");

  return result;
}

TableRowProperties ResolvedStyle::toTableRowProperties() const {
  TableRowProperties result;

  // TODO

  return result;
}

TableCellProperties ResolvedStyle::toTableCellProperties() const {
  TableCellProperties result;

  result.padding = lookup(tableCellProperties, "fo:padding");
  result.paddingRect = lookupRect(tableCellProperties, "fo:padding-");
  result.border = lookup(tableCellProperties, "fo:border");
  result.borderRect = lookupRect(tableCellProperties, "fo:border-");

  return result;
}

DrawingProperties ResolvedStyle::toDrawingProperties() const {
  DrawingProperties result;

  result.strokeWidth = lookup(graphicProperties, "svg:stroke-width");
  result.strokeColor = lookup(graphicProperties, "svg:stroke-color");
  result.fillColor = lookup(graphicProperties, "draw:fill-color");
  result.verticalAlign =
      lookup(graphicProperties, "draw:textarea-vertical-align");

  return result;
}

Style::Style(std::shared_ptr<Style> parent, pugi::xml_node styleNode)
    : m_parent{std::move(parent)}, m_styleNode{styleNode} {}

ResolvedStyle Style::resolve() const {
  ResolvedStyle result;

  if (m_parent)
    result = m_parent->resolve();

  // TODO some property nodes have children e.g. <style:paragraph-properties>
  // TODO some properties use relative measures of their parent's properties
  // e.g. fo:font-size

  attributesToMap(m_styleNode.child("style:paragraph-properties"),
                  result.paragraphProperties);
  attributesToMap(m_styleNode.child("style:text-properties"),
                  result.textProperties);

  attributesToMap(m_styleNode.child("style:table-properties"),
                  result.tableProperties);
  attributesToMap(m_styleNode.child("style:table-column-properties"),
                  result.tableColumnProperties);
  attributesToMap(m_styleNode.child("style:table-row-properties"),
                  result.tableRowProperties);
  attributesToMap(m_styleNode.child("style:table-cell-properties"),
                  result.tableCellProperties);

  attributesToMap(m_styleNode.child("style:chart-properties"),
                  result.chartProperties);
  attributesToMap(m_styleNode.child("style:drawing-page-properties"),
                  result.drawingPageProperties);
  attributesToMap(m_styleNode.child("style:graphic-properties"),
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

PageProperties Styles::pageProperties(const std::string &name) const {
  auto pageLayoutIt = m_indexPageLayout.find(name);
  if (pageLayoutIt == m_indexPageLayout.end())
    throw 1; // TODO exception or optional
  auto pageLayoutProp =
      pageLayoutIt->second.child("style:page-layout-properties");

  PageProperties result;

  result.width = Property(std::make_shared<common::XmlAttributeProperty>(
      pageLayoutProp.attribute("fo:page-width")));
  result.height = Property(std::make_shared<common::XmlAttributeProperty>(
      pageLayoutProp.attribute("fo:page-height")));
  result.marginTop = Property(std::make_shared<common::XmlAttributeProperty>(
      pageLayoutProp.attribute("fo:margin-top")));
  result.marginBottom = Property(std::make_shared<common::XmlAttributeProperty>(
      pageLayoutProp.attribute("fo:margin-bottom")));
  result.marginLeft = Property(std::make_shared<common::XmlAttributeProperty>(
      pageLayoutProp.attribute("fo:margin-left")));
  result.marginRight = Property(std::make_shared<common::XmlAttributeProperty>(
      pageLayoutProp.attribute("fo:margin-right")));
  result.printOrientation =
      Property(std::make_shared<common::XmlAttributeProperty>(
          pageLayoutProp.attribute("style:print-orientation")));

  return result;
}

PageProperties Styles::masterPageProperties(const std::string &name) const {
  auto masterPageIt = m_indexMasterPage.find(name);
  if (masterPageIt == m_indexMasterPage.end())
    throw 1; // TODO exception or optional
  const std::string pageLayoutName =
      masterPageIt->second.attribute("style:page-layout-name").value();
  return pageProperties(pageLayoutName);
}

PageProperties Styles::defaultPageProperties() const {
  const pugi::xml_node masterStyles =
      m_stylesRoot.child("office:master-styles");
  const pugi::xml_node masterStyle = masterStyles.first_child();
  const std::string pageLayoutName =
      masterStyle.attribute("style:page-layout-name").value();
  return pageProperties(pageLayoutName);
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
