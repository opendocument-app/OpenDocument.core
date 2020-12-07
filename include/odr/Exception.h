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

struct NoImageFile : public std::runtime_error {
  NoImageFile() : std::runtime_error("not an image file") {}
};

struct NoDocumentFile : public std::runtime_error {
  NoDocumentFile() : std::runtime_error("not a document file") {}
};

} // namespace odr

#endif // ODR_EXCEPTION_H
