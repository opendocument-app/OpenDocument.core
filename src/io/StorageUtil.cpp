#include "StorageUtil.h"
#include "Storage.h"

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

void StorageUtil::deepVisit(const Storage &storage, Storage::Visitor visitor) {
    deepVisit(storage, "", visitor);
}

void StorageUtil::deepVisit(const Storage &storage, const Path &path, Storage::Visitor visitor) {
    visitor(path);
    storage.visit(path, [&](const Path &child) {
        visitor(child);
        if (storage.isFolder(child)) deepVisit(storage, child, visitor);
    });
}

}
