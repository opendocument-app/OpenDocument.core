#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <any>
#include <internal/abstract/element.h>
#include <memory>
#include <odr/document.h>
#include <odr/element.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr::internal::odf {

odr::Element create_default_element(pugi::xml_node node,
                                    OpenDocument *document);

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
