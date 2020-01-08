#include "StreamUtil.h"
#include "Stream.h"

namespace odr {

std::string StreamUtil::read(Source &in) {
    // TODO use available? or enhance Source with tell?
    static constexpr std::uint32_t bufferSize = 4096;

    std::string result;
    char buffer[bufferSize];

    while (true) {
        const std::uint32_t read = in.read(buffer, bufferSize);
        if (read == 0) break;
        result.append(buffer, read);
    }

    return result;
}

}
