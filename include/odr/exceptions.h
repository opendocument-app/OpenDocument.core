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

struct FileNotCreated final : public std::runtime_error {
  FileNotCreated() : std::runtime_error("file not created") {}
};

struct UnknownFileType final : public std::runtime_error {
  UnknownFileType() : std::runtime_error("unknown file type") {}
};

struct FileReadError final : public std::runtime_error {
  FileReadError() : std::runtime_error("file read error") {}
};

struct NoZipFile : public std::runtime_error {
  NoZipFile() : std::runtime_error("not a zip file") {}
};

struct ZipSaveError : public std::runtime_error {
  ZipSaveError() : std::runtime_error("zip save error") {}
};

struct CfbError : public std::runtime_error {
  explicit CfbError(const char *desc) : std::runtime_error(desc) {}
};

struct NoCfbFile : public CfbError {
  NoCfbFile() : CfbError("no cfb file") {}
};

struct CfbFileCorrupted : public CfbError {
  CfbFileCorrupted() : CfbError("cfb file corrupted") {}
};

struct NoImageFile final : public std::runtime_error {
  NoImageFile() : std::runtime_error("not an image file") {}
};

struct NoDocumentFile final : public std::runtime_error {
  NoDocumentFile() : std::runtime_error("not a document file") {}
};

struct NoOpenDocumentFile final : public std::runtime_error {
  NoOpenDocumentFile() : std::runtime_error("not an open document file") {}
};

struct NoXml final : public std::runtime_error {
  NoXml() : std::runtime_error("not xml") {}
};

struct PropertyNotOptional final : public std::runtime_error {
  PropertyNotOptional() : std::runtime_error("property not optional") {}
};

struct PropertyReadOnly final : public std::runtime_error {
  PropertyReadOnly() : std::runtime_error("property is readonly") {}
};

struct UnsupportedCryptoAlgorithm final : public std::runtime_error {
  UnsupportedCryptoAlgorithm()
      : std::runtime_error("unsupported crypto algorithm") {}
};

} // namespace odr

#endif // ODR_EXCEPTIONS_H
