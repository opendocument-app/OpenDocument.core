#include <access/Path.h>
#include <access/SystemStorage.h>

namespace odr {
namespace access {

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

bool SystemStorage::isDirectory(const Path &) const {
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

bool SystemStorage::createDirectory(const Path &) const {
  return false; // TODO
}

void SystemStorage::visit(Visitor) const {
  // TODO
}

std::unique_ptr<std::istream> SystemStorage::read(const Path &) const {
  return nullptr; // TODO
}

std::unique_ptr<std::ostream> SystemStorage::write(const Path &) const {
  return nullptr; // TODO
}

} // namespace access
} // namespace odr
