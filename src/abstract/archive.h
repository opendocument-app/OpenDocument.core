#ifndef ODR_ABSTRACT_ARCHIVE_H
#define ODR_ABSTRACT_ARCHIVE_H

#include <memory>

namespace odr::common {
class Path;
} // namespace odr::common

namespace odr::abstract {
class Filesystem;

class Archive {
public:
  virtual ~Archive() = default;

  [[nodiscard]] virtual std::shared_ptr<Filesystem> filesystem() const = 0;

  virtual void save(const common::Path &path) const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_ARCHIVE_H
