#include <access/ChildStorage.h>

namespace odr {
namespace access {

ChildStorage::ChildStorage(const Storage &parent, const Path prefix)
    : parent_(parent), prefix_(prefix) {
  // TODO throw if path is not a folder in parent
}

// TODO throw on escaping path

bool ChildStorage::isSomething(const Path &path) const {
  return parent_.isSomething(prefix_.join(path));
}

bool ChildStorage::isFile(const Path &path) const {
  return parent_.isFile(prefix_.join(path));
}

bool ChildStorage::isDirectory(const Path &path) const {
  return parent_.isFile(prefix_.join(path));
}

bool ChildStorage::isReadable(const Path &path) const {
  return parent_.isFile(prefix_.join(path));
}

bool ChildStorage::isWriteable(const Path &path) const {
  return parent_.isFile(prefix_.join(path));
}

std::uint64_t ChildStorage::size(const Path &path) const {
  return parent_.size(prefix_.join(path));
}

bool ChildStorage::remove(const Path &path) const {
  return parent_.remove(prefix_.join(path));
}

bool ChildStorage::copy(const Path &from, const Path &to) const {
  return parent_.copy(prefix_.join(from), prefix_.join(to));
}

bool ChildStorage::move(const Path &from, const Path &to) const {
  return parent_.move(prefix_.join(from), prefix_.join(to));
}

bool ChildStorage::createDirectory(const Path &path) const {
  return parent_.createDirectory(prefix_.join(path));
}

void ChildStorage::visit(Visitor visiter) const {
  parent_.visit([&](const auto &p) {
    // TODO show only subdir results
    // TODO remove prefix_
    visiter(p);
  });
}

std::unique_ptr<std::istream> ChildStorage::read(const Path &path) const {
  return parent_.read(prefix_.join(path));
}

std::unique_ptr<std::ostream> ChildStorage::write(const Path &path) const {
  return parent_.write(prefix_.join(path));
}

} // namespace access
} // namespace odr
