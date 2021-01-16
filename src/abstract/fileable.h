#ifndef ODR_ABSTRACT_FILEABLE_H
#define ODR_ABSTRACT_FILEABLE_H

#include <memory>
#include <ostream>

namespace odr::abstract {
class File;

class Fileable {
public:
  virtual ~Fileable() = default;

  virtual void save(std::ostream &) const = 0;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_FILEABLE_H
