#include <access/StorageUtil.h>
#include <access/Storage.h>

namespace odr {

std::string StorageUtil::read(const Storage &storage, const Path &path) {
    static constexpr std::uint32_t bufferSize = 4096;

    std::string result;
    char buffer[bufferSize];
    auto in = storage.read(path);

    result.reserve(storage.size(path));
    while (true) {
        const std::uint32_t read = in->read(buffer, bufferSize);
        if (read == 0) break;
        result.append(buffer, read);
    }

    return result;
}

}
