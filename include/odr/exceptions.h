#ifndef ODR_EXCEPTIONS_H
#define ODR_EXCEPTIONS_H

#include <stdexcept>

namespace odr {

struct UnsupportedOperation final : public std::runtime_error {
  UnsupportedOperation() : std::runtime_error("unsupported operation") {}
};

struct FileNotFound final : public std::runtime_error {
  FileNotFound() : std::runtime_error("file not found") {}
  explicit FileNotFound(const char *desc) : std::runtime_error(desc) {}
};

struct UnknownFileType final : public std::runtime_error {
  UnknownFileType() : std::runtime_error("unknown file type") {}
};

struct NoZipFile : public std::runtime_error {
  NoZipFile() : std::runtime_error("not a zip file") {}
};

struct NoImageFile final : public std::runtime_error {
  NoImageFile() : std::runtime_error("not an image file") {}
};

struct NoDocumentFile final : public std::runtime_error {
  NoDocumentFile() : std::runtime_error("not a document file") {}
};

struct PropertyNotOptional final : public std::runtime_error {
  PropertyNotOptional() : std::runtime_error("property not optional") {}
};

struct PropertyReadOnly final : public std::runtime_error {
  PropertyReadOnly() : std::runtime_error("property is readonly") {}
};

} // namespace odr

#endif // ODR_EXCEPTIONS_H
