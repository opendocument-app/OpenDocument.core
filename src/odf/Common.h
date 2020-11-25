#ifndef ODR_ODF_COMMON_H
#define ODR_ODF_COMMON_H

namespace pugi {
class xml_document;
}

namespace odr {
struct PageProperties;
namespace odf {

namespace Common {
PageProperties pageProperties(const pugi::xml_document &style);
}

}
}

#endif // ODR_ODF_COMMON_H
