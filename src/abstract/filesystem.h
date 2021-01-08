#ifndef ODR_ABSTRACT_FILESYSTEM_H
#define ODR_ABSTRACT_FILESYSTEM_H

namespace odr::common {
class File;
class Path;
} // namespace odr::common

namespace odr::abstract {

class Filesystem {
public:
  virtual ~Filesystem() = default;

  virtual std::shared_prt<File> open(Path path) const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_FILESYSTEM_H
