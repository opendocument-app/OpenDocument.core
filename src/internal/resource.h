#ifndef ODR_INTERNAL_RESOURCE_H
#define ODR_INTERNAL_RESOURCE_H

#include <internal/abstract/filesystem.h>

#include <memory>
#include <vector>

namespace odr::internal {

struct Resource {
  const char *path;
  const char *data;
  std::size_t size;
};

class Resources final {
public:
  static const Resources &instance();

  const std::vector<Resource> &resources() const;
  std::shared_ptr<const abstract::Filesystem> filesystem() const;

private:
  Resources();

  std::vector<Resource> m_resources;
  std::shared_ptr<const abstract::Filesystem> m_filesystem;
};

} // namespace odr::internal

#endif // ODR_INTERNAL_RESOURCE_H
