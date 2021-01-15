#ifndef ODR_ABSTRACT_FILEABLE_H
#define ODR_ABSTRACT_FILEABLE_H

namespace odr::common {
class Path;
}

namespace odr::abstract {

class Fileable {
public:
  virtual ~Fileable() = default;

  void save(const common::Path &path) const final;
};

} // namespace odr::abstract

#endif // ODR_ABSTRACT_FILEABLE_H
