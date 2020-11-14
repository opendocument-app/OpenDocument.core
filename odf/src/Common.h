#ifndef ODR_ODF_COMMON_H
#define ODR_ODF_COMMON_H

#include <odr/GenericDocument.h>
#include <pugixml.hpp>

namespace odr::odf {

namespace Common {

GenericTextDocument::PageProperties pageProperties(const pugi::xml_document &style);

}

}

#endif // ODR_ODF_COMMON_H
