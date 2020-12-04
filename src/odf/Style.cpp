#include <cstring>
#include <odf/Style.h>
#include <odr/Document.h>

namespace odr::odf {

Style::Style(pugi::xml_document styles, pugi::xml_node contentRoot)
    : m_styles{std::move(styles)}, m_contentRoot{contentRoot} {}

PageProperties Style::defaultPageProperties() const {
  const pugi::xml_node masterStyles =
      m_styles.root().child("office:master-styles");
  const pugi::xml_node masterStyle = masterStyles.first_child();
  const std::string pageLayoutName =
      masterStyle.attribute("style:page-layout-name").value();
  return pageProperties(pageLayoutName);
}

PageProperties Style::pageProperties(const std::string &name) const {
  PageProperties result;

  auto pageLayoutIt = m_indexPageLayout.find(name);
  if (pageLayoutIt == m_indexPageLayout.end())
    throw 1; // TODO
  auto &&pageLayout = pageLayoutIt->second;

  result.width = pageLayout.attribute("fo:page-width").value();
  result.height = pageLayout.attribute("fo:page-height").value();
  result.marginTop = pageLayout.attribute("fo:margin-top").value();
  result.marginBottom = pageLayout.attribute("fo:margin-bottom").value();
  result.marginLeft = pageLayout.attribute("fo:margin-left").value();
  result.marginRight = pageLayout.attribute("fo:margin-right").value();
  result.printOrientation =
      pageLayout.attribute("style:print-orientation").value();

  return result;
}

void Style::generateIndices() {
  if (auto fontFaceDecls = m_styles.root().child("office:font-face-decls");
      fontFaceDecls) {
    generateIndices(fontFaceDecls);
  }

  if (auto styles = m_styles.root().child("office:styles"); styles) {
    generateIndices(styles);
  }

  if (auto automaticStyles = m_styles.root().child("office:automatic-styles");
      automaticStyles) {
    generateIndices(automaticStyles);
  }

  if (auto masterStyles = m_styles.root().child("office:master-styles");
      masterStyles) {
    generateIndices(masterStyles);
  }

  // content styles

  if (auto fontFaceDecls = m_contentRoot.root().child("office:font-face-decls");
      fontFaceDecls) {
    generateIndices(fontFaceDecls);
  }

  if (auto automaticStyles =
          m_contentRoot.root().child("office:automatic-styles");
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

} // namespace odr::odf
