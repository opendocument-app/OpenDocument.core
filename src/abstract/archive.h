#ifndef ODR_ABSTRACT_ARCHIVE_H
#define ODR_ABSTRACT_ARCHIVE_H

namespace odr::abstract {
class Filesystem;

class Archive {
public:
  virtual ~Archive() = default;

  virtual std::shared_ptr<Filesystem> filesystem() const = 0;

  virtual void save(const common::Path &path) const = 0;
};

}

#endif // ODR_ABSTRACT_ARCHIVE_H
