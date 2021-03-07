#ifndef ODR_EXPERIMENTAL_EXCEPTIONS_H
#define ODR_EXPERIMENTAL_EXCEPTIONS_H

#include <stdexcept>

namespace odr::experimental {

struct UnsupportedOperation final : public std::runtime_error {
  UnsupportedOperation();
};

struct FileNotFound final : public std::runtime_error {
  FileNotFound();
};

struct FileNotCreated final : public std::runtime_error {
  FileNotCreated();
};

struct UnknownFileType final : public std::runtime_error {
  UnknownFileType();
};

struct FileReadError final : public std::runtime_error {
  FileReadError();
};

struct NoZipFile : public std::runtime_error {
  NoZipFile();
};

struct ZipSaveError : public std::runtime_error {
  ZipSaveError();
};

struct CfbError : public std::runtime_error {
  explicit CfbError(const char *desc);
};

struct NoCfbFile : public CfbError {
  NoCfbFile();
};

struct CfbFileCorrupted : public CfbError {
  CfbFileCorrupted();
};

struct NoImageFile final : public std::runtime_error {
  NoImageFile();
};

struct NoDocumentFile final : public std::runtime_error {
  NoDocumentFile();
};

struct NoOpenDocumentFile final : public std::runtime_error {
  NoOpenDocumentFile();
};

struct NoXml final : public std::runtime_error {
  NoXml();
};

struct PropertyNotOptional final : public std::runtime_error {
  PropertyNotOptional();
};

struct PropertyReadOnly final : public std::runtime_error {
  PropertyReadOnly();
};

struct UnsupportedCryptoAlgorithm final : public std::runtime_error {
  UnsupportedCryptoAlgorithm();
};

struct NoSvmFile : public std::runtime_error {
  NoSvmFile();
};

struct MalformedSvmFile : public std::runtime_error {
  MalformedSvmFile();
};

struct UnsupportedEndian final : public std::runtime_error {
  UnsupportedEndian();
};

struct MsUnsupportedCryptoAlgorithm final : public std::runtime_error {
  MsUnsupportedCryptoAlgorithm();
};

} // namespace odr::experimental

#endif // ODR_EXPERIMENTAL_EXCEPTIONS_H
