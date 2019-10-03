#ifndef ODR_STRINGUTIL_H
#define ODR_STRINGUTIL_H

#include <string>

namespace odr {

namespace StringUtil {
extern void findAndReplaceAll(std::string &data, const std::string &toSearch, const std::string &replaceStr);
}

}

#endif //ODR_STRINGUTIL_H
