#ifndef ODR_COMMON_FILE_H
#define ODR_COMMON_FILE_H

namespace odr {
struct FileMeta;

namespace common {

class File {
public:
  virtual ~File() = default;

  virtual const FileMeta &meta() const noexcept = 0;
};

}
}

#endif // ODR_COMMON_FILE_H
