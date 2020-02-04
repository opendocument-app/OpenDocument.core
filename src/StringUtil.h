#ifndef ODR_STRINGUTIL_H
#define ODR_STRINGUTIL_H

#include <string>

namespace odr {

namespace StringUtil {
extern bool startsWith(const std::string &, const std::string &);
extern bool endsWith(const std::string &, const std::string &);
extern void findAndReplaceAll(std::string &data, const std::string &toSearch, const std::string &replaceStr);
}

}

#endif //ODR_STRINGUTIL_H
