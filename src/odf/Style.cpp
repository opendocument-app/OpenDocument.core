#include <odf/Style.h>

namespace odr::odf {

Style::Style(pugi::xml_document styles, pugi::xml_node content_styles)
    : m_styles{std::move(styles)}, m_content_styles{content_styles} {}

} // namespace odr::odf
