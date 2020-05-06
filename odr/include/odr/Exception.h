#ifndef ODR_EXCEPTION_H
#define ODR_EXCEPTION_H

namespace odr {

struct UnsupportedOperation : public std::runtime_error {
  UnsupportedOperation() : std::runtime_error("unsupported operation") {}
};

struct FileNotFound : public std::runtime_error {
  FileNotFound() : std::runtime_error("file not found") {}
  explicit FileNotFound(const char *desc) : std::runtime_error(desc) {}
};

struct UnknownFileType : public std::runtime_error {
  UnknownFileType() : std::runtime_error("unknown file type") {}
};

} // namespace odr

#endif // ODR_EXCEPTION_H
