#ifndef ODR_EXCEPTION_H
#define ODR_EXCEPTION_H

namespace odr {

struct UnknownFileType : public std::runtime_error {
  UnknownFileType() : std::runtime_error("unknown file type") {}
};

struct UnsupportedOperation : public std::runtime_error {
  UnsupportedOperation() : std::runtime_error("unsupported operation") {}
};

} // namespace odr

#endif // ODR_EXCEPTION_H
