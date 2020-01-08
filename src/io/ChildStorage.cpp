#include "ChildStorage.h"
#include "Stream.h"

namespace odr {

ChildStorage::ChildStorage(const Storage &parent, const Path prefix) :
        parent(parent),
        prefix(prefix) {
    // TODO throw if path is not a folder in parent
}

// TODO throw on escaping path

bool ChildStorage::isSomething(const Path &path) const {
    return parent.isSomething(prefix.join(path));
}

bool ChildStorage::isFile(const Path &path) const {
    return parent.isFile(prefix.join(path));
}

bool ChildStorage::isFolder(const Path &path) const {
    return parent.isFile(prefix.join(path));
}

bool ChildStorage::isReadable(const Path &path) const {
    return parent.isFile(prefix.join(path));
}

bool ChildStorage::isWriteable(const Path &path) const {
    return parent.isFile(prefix.join(path));
}

std::uint64_t ChildStorage::size(const Path &path) const {
    return parent.size(prefix.join(path));
}

bool ChildStorage::remove(const Path &path) const {
    return parent.remove(prefix.join(path));
}

bool ChildStorage::copy(const Path &from, const Path &to) const {
    return parent.copy(prefix.join(from), prefix.join(to));
}

bool ChildStorage::move(const Path &from, const Path &to) const {
    return parent.move(prefix.join(from), prefix.join(to));
}

void ChildStorage::visit(const Path &path, Visiter visiter) const {
    parent.visit(prefix.join(path), [&](const auto &p) {
        // TODO remove prefix
        visiter(p);
    });
}

std::unique_ptr<Source> ChildStorage::read(const Path &path) const {
    return parent.read(prefix.join(path));
}

std::unique_ptr<Sink> ChildStorage::write(const Path &path) const {
    return parent.write(prefix.join(path));
}

}
