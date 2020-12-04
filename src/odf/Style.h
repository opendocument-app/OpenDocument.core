#ifndef ODR_ODF_STYLE_H
#define ODR_ODF_STYLE_H

#include <pugixml.hpp>

namespace odr::odf {

class Style {
public:
  Style(pugi::xml_document styles, pugi::xml_node content_styles);

private:
  pugi::xml_document m_styles;
  pugi::xml_node m_content_styles;
};

} // namespace odr::odf

#endif // ODR_ODF_STYLE_H
