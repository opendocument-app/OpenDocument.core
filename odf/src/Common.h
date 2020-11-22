#ifndef ODR_ODF_COMMON_H
#define ODR_ODF_COMMON_H

#include <pugixml.hpp>

namespace odr {
struct PageProperties;
namespace odf {

namespace Common {

PageProperties pageProperties(const pugi::xml_document &style);

}

}
}

#endif // ODR_ODF_COMMON_H
