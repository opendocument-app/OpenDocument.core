#include <Common.h>

namespace odr::odf {

common::GenericTextDocument::PageProperties Common::pageProperties(const pugi::xml_document &style) {
  common::GenericTextDocument::PageProperties result;

  const pugi::xml_node masterStyles = style.child("office:document-styles")
      .child("office:master-styles");
  const pugi::xml_node automaticStyles = style.child("office:document-styles").child("office:automatic-styles");
  const pugi::xml_node masterStyle = masterStyles.first_child();

  const std::string pageLayoutName = masterStyle.attribute("style:page-layout-name").value();

  for (auto &&n : automaticStyles.children("style:page-layout")) {
    if (n.attribute("style:name").value() == pageLayoutName) {
      const pugi::xml_node properties = n.child("style:page-layout-properties");
      result.width = properties.attribute("fo:page-width").value();
      result.height = properties.attribute("fo:page-height").value();
      result.marginTop = properties.attribute("fo:margin-top").value();
      result.marginBottom = properties.attribute("fo:margin-bottom").value();
      result.marginLeft = properties.attribute("fo:margin-left").value();
      result.marginRight = properties.attribute("fo:margin-right").value();
      result.printOrientation = properties.attribute("style:print-orientation").value();
      break;
    }
  }

  return result;
}

}
