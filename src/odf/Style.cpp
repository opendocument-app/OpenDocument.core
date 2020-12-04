#include <cstring>
#include <odf/Style.h>
#include <odr/Document.h>

namespace odr::odf {

Style::Style(pugi::xml_document styles, pugi::xml_node contentRoot)
    : m_styles{std::move(styles)}, m_contentRoot{contentRoot} {
  generateIndices();
}

ParagraphProperties Style::paragraphProperties(const std::string &name) const {
  auto styleIt = m_indexStyle.find(name);
  if (styleIt == m_indexStyle.end())
    throw 1; // TODO exception or optional
  auto paragraphProp = styleIt->second.child("style:paragraph-properties");

  ParagraphProperties result;

  // TODO recurse and assign

  return result;
}

TextProperties Style::textProperties(const std::string &name) const {
  return {}; // TODO
}

PageProperties Style::defaultPageProperties() const {
  const pugi::xml_node masterStyles =
      m_styles.document_element().child("office:master-styles");
  const pugi::xml_node masterStyle = masterStyles.first_child();
  const std::string pageLayoutName =
      masterStyle.attribute("style:page-layout-name").value();
  return pageProperties(pageLayoutName);
}

PageProperties Style::pageProperties(const std::string &name) const {
  auto pageLayoutIt = m_indexPageLayout.find(name);
  if (pageLayoutIt == m_indexPageLayout.end())
    throw 1; // TODO exception or optional
  auto pageLayoutProp = pageLayoutIt->second.child("style:page-layout-properties");

  PageProperties result;

  result.width = pageLayoutProp.attribute("fo:page-width").value();
  result.height = pageLayoutProp.attribute("fo:page-height").value();
  result.marginTop = pageLayoutProp.attribute("fo:margin-top").value();
  result.marginBottom = pageLayoutProp.attribute("fo:margin-bottom").value();
  result.marginLeft = pageLayoutProp.attribute("fo:margin-left").value();
  result.marginRight = pageLayoutProp.attribute("fo:margin-right").value();
  result.printOrientation =
      pageLayoutProp.attribute("style:print-orientation").value();

  return result;
}

void Style::generateIndices() {
  if (auto fontFaceDecls = m_styles.document_element().child("office:font-face-decls");
      fontFaceDecls) {
    generateIndices(fontFaceDecls);
  }

  if (auto styles = m_styles.document_element().child("office:styles"); styles) {
    generateIndices(styles);
  }

  if (auto automaticStyles = m_styles.document_element().child("office:automatic-styles");
      automaticStyles) {
    generateIndices(automaticStyles);
  }

  if (auto masterStyles = m_styles.document_element().child("office:master-styles");
      masterStyles) {
    generateIndices(masterStyles);
  }

  // content styles

  if (auto fontFaceDecls = m_contentRoot.child("office:font-face-decls");
      fontFaceDecls) {
    generateIndices(fontFaceDecls);
  }

  if (auto automaticStyles =
          m_contentRoot.child("office:automatic-styles");
      automaticStyles) {
    generateIndices(automaticStyles);
  }
}

void Style::generateIndices(pugi::xml_node node) {
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

std::vector<pugi::xml_node> Style::styleHierarchy(std::string name) const {
  std::vector<pugi::xml_node> result;

  while (true) {
    const auto styleIt = m_indexStyle.find(name);
    if (styleIt == m_indexStyle.end()) break;

    const auto style = styleIt->second;
    result.push_back(style);

    const auto parent = style.attribute("style:parent-style-name");
    if (!parent) {
      const auto family = style.attribute("style:family-name");
      if (family) {
        const auto defaultStyleIt = m_indexDefaultStyle.find(name);
        if (defaultStyleIt != m_indexDefaultStyle.end())
          result.push_back(defaultStyleIt->second);
      }
      break;
    }
    name = parent.value();
  }

  return result;
}

} // namespace odr::odf
