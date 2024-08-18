#ifndef ODR_INTERNAL_RESOURCE_HPP
#define ODR_INTERNAL_RESOURCE_HPP

#include <odr/internal/abstract/filesystem.hpp>

#include <memory>
#include <vector>

namespace odr {
class File;
class Filesystem;
} // namespace odr

namespace odr::internal {

struct Resource {
  const char *path{};
  const char *data{};
  std::size_t size{};
};

class Resources final {
public:
  static const std::vector<Resource> &resources();
  static Filesystem filesystem();
  static File open(const common::Path &path);

private:
  Resources();

  static const Resources &instance();

  std::vector<Resource> m_resources;
  std::shared_ptr<abstract::Filesystem> m_filesystem;
};

} // namespace odr::internal

#endif // ODR_INTERNAL_RESOURCE_HPP
