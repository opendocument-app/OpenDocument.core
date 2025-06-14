#pragma once

#include <memory>

namespace odr::internal::common {
class Path;
} // namespace odr::internal::common

namespace odr::internal::abstract {
class Filesystem;

class Archive {
public:
  virtual ~Archive() = default;

  [[nodiscard]] virtual std::shared_ptr<Filesystem> as_filesystem() const = 0;

  virtual void save(std::ostream &out) const = 0;
};

} // namespace odr::internal::abstract
