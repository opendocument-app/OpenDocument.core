#ifndef ODR_INTERNAL_EXCEPTIONS_H
#define ODR_INTERNAL_EXCEPTIONS_H

#include <stdexcept>

namespace odr::internal {

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

struct NoZipFile final : public std::runtime_error {
  NoZipFile();
};

struct ZipSaveError final : public std::runtime_error {
  ZipSaveError();
};

struct CfbError : public std::runtime_error {
  CfbError(const char *desc);
};

struct NoCfbFile final : public CfbError {
  NoCfbFile();
};

struct CfbFileCorrupted final : public CfbError {
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

struct NoSvmFile final : public std::runtime_error {
  NoSvmFile();
};

struct MalformedSvmFile final : public std::runtime_error {
  MalformedSvmFile();
};

struct UnsupportedEndian final : public std::runtime_error {
  UnsupportedEndian();
};

struct MsUnsupportedCryptoAlgorithm final : public std::runtime_error {
  MsUnsupportedCryptoAlgorithm();
};

} // namespace odr::internal

#endif // ODR_INTERNAL_EXCEPTIONS_H
