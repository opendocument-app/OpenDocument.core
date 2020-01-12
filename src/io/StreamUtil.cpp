#include "StreamUtil.h"
#include <cstring>
#include "Stream.h"

namespace odr {

static constexpr std::uint32_t bufferSize = 4096;

std::uint32_t StringSource::read(char *d, std::uint32_t amount) {
    amount = std::min(amount, available());
    std::memcpy(d, &data[pos], amount);
    pos += amount;
    return amount;
}

std::uint32_t StringSource::available() const {
    return data.size() - pos;
}

std::string StreamUtil::read(Source &in) {
    // TODO use available? or enhance Source with tell?

    std::string result;
    char buffer[bufferSize];

    while (true) {
        const std::uint32_t read = in.read(buffer, bufferSize);
        if (read == 0) break;
        result.append(buffer, read);
    }

    return result;
}

void StreamUtil::pipe(Source &in, Sink &out) {
    char buffer[bufferSize];

    while (true) {
        const std::uint32_t read = in.read(buffer, bufferSize);
        if (read == 0) break;
        out.write(buffer, read);
    }
}

}
