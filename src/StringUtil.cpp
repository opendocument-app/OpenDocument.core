#include "StringUtil.h"

namespace odr {

bool StringUtil::startsWith(const std::string &a, const std::string &b) {
    return a.rfind(b, 0) == 0;
}

bool StringUtil::endsWith(const std::string &a, const std::string &b) {
    return (a.length() >= b.length()) && (a.compare(a.length() - b.length(), b.length(), b) == 0);
}

void StringUtil::findAndReplaceAll(std::string &data, const std::string &toSearch, const std::string &replaceStr) {
    std::size_t pos = data.find(toSearch);
    while (pos != std::string::npos) {
        data.replace(pos, toSearch.size(), replaceStr);
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

}
