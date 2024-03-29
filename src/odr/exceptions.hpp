#ifndef ODR_EXCEPTIONS_HPP
#define ODR_EXCEPTIONS_HPP

#include <stdexcept>

namespace odr {
enum class FileType;

struct UnsupportedOperation final : public std::runtime_error {
  UnsupportedOperation();
};

struct FileNotFound final : public std::runtime_error {
  FileNotFound();
};

struct UnknownFileType final : public std::runtime_error {
  UnknownFileType();
};

struct UnsupportedFileType final : public std::runtime_error {
  FileType file_type;

  explicit UnsupportedFileType(FileType file_type);
};

struct FileReadError final : public std::runtime_error {
  FileReadError();
};

struct FileWriteError final : public std::runtime_error {
  FileWriteError();
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

struct NoTextFile final : public std::runtime_error {
  NoTextFile();
};

struct UnknownCharset final : public std::runtime_error {
  UnknownCharset();
};

struct NoImageFile final : public std::runtime_error {
  NoImageFile();
};

struct NoArchiveFile final : public std::runtime_error {
  NoArchiveFile();
};

struct NoDocumentFile final : public std::runtime_error {
  NoDocumentFile();
};

struct NoOpenDocumentFile final : public std::runtime_error {
  NoOpenDocumentFile();
};

struct NoOfficeOpenXmlFile final : public std::runtime_error {
  NoOfficeOpenXmlFile();
};

struct NoPdfFile final : public std::runtime_error {
  NoPdfFile();
};

struct NoXml final : public std::runtime_error {
  NoXml();
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

struct WrongPassword final : public std::runtime_error {
  WrongPassword();
};

struct UnknownDocumentType final : public std::runtime_error {
  UnknownDocumentType();
};

} // namespace odr

#endif // ODR_EXCEPTIONS_HPP
