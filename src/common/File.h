#ifndef ODR_COMMON_FILE_H
#define ODR_COMMON_FILE_H

#include <string>

namespace odr {
struct FileMeta;

namespace access {
class Path;
}

namespace common {

class File {
public:
  virtual ~File() = default;

  virtual bool editable() const noexcept;
  virtual bool savable(bool encrypted) const noexcept;

  virtual const FileMeta &meta() const noexcept = 0;

  virtual void save(const access::Path &path) const;
  virtual void save(const access::Path &path,
                    const std::string &password) const;
};

}
}

#endif // ODR_COMMON_FILE_H
