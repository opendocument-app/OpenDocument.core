#include "StringUtil.h"

namespace odr {

void StringUtil::findAndReplaceAll(std::string &data, const std::string &toSearch, const std::string &replaceStr) {
    std::size_t pos = data.find(toSearch);
    while (pos != std::string::npos) {
        data.replace(pos, toSearch.size(), replaceStr);
        pos = data.find(toSearch, pos + replaceStr.size());
    }
}

}
