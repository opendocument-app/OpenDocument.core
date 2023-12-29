#ifndef ODR_INTERNAL_ABSTRACT_ARCHIVE_HPP
#define ODR_INTERNAL_ABSTRACT_ARCHIVE_HPP

#include <memory>

namespace odr::internal::common {
class Path;
} // namespace odr::internal::common

namespace odr::internal::abstract {
class Filesystem;

class Archive {
public:
  virtual ~Archive() = default;

  [[nodiscard]] virtual std::shared_ptr<Filesystem> filesystem() const = 0;

  virtual void save(const common::Path &path) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_ARCHIVE_HPP
