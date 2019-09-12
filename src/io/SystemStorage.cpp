#include "SystemStorage.h"
#include "Path.h"
#include "Stream.h"

namespace odr {

const SystemStorage &SystemStorage::instance() {
    static SystemStorage instance;
    return instance;
}

bool SystemStorage::isSomething(const Path &) const {
    return false; // TODO
}

bool SystemStorage::isFile(const Path &) const {
    return false; // TODO
}

bool SystemStorage::isFolder(const Path &) const {
    return false; // TODO
}

bool SystemStorage::isReadable(const Path &) const {
    return false; // TODO
}

bool SystemStorage::isWriteable(const Path &) const {
    return false; // TODO
}

std::uint64_t SystemStorage::size(const Path &) const {
    return 0; // TODO
}

bool SystemStorage::remove(const Path &) const {
    return false; // TODO
}

bool SystemStorage::copy(const Path &, const Path &) const {
    return false; // TODO
}

bool SystemStorage::move(const Path &, const Path &) const {
    return false; // TODO
}

void SystemStorage::visit(const Path &, Visiter) const {
    // TODO
}

std::unique_ptr<Source> SystemStorage::read(const Path &) const {
    return nullptr; // TODO
}

std::unique_ptr<Sink> SystemStorage::write(const Path &) const {
    return nullptr; // TODO
}

}
