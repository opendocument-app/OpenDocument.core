#ifndef ODR_EXCEPTIONS_HPP
#define ODR_EXCEPTIONS_HPP

#include <stdexcept>

namespace odr {
enum class FileType;

/// @brief Unsupported operation exception
struct UnsupportedOperation final : public std::runtime_error {
  UnsupportedOperation();
};

/// @brief File not found exception
struct FileNotFound final : public std::runtime_error {
  FileNotFound();
};

/// @brief Unknown file type exception
struct UnknownFileType final : public std::runtime_error {
  UnknownFileType();
};

/// @brief Unsupported file type exception
struct UnsupportedFileType final : public std::runtime_error {
  FileType file_type;

  explicit UnsupportedFileType(FileType file_type);
};

/// @brief Unknown decoder engine exception
struct UnknownDecoderEngine final : public std::runtime_error {
  UnknownDecoderEngine();
};

/// @brief File read error
struct FileReadError final : public std::runtime_error {
  FileReadError();
};

/// @brief File write error
struct FileWriteError final : public std::runtime_error {
  FileWriteError();
};

/// @brief No ZIP file exception base
struct NoZipFile : public std::runtime_error {
  NoZipFile();
};

/// @brief ZIP save error base
struct ZipSaveError : public std::runtime_error {
  ZipSaveError();
};

/// @brief CFB error base
struct CfbError : public std::runtime_error {
  explicit CfbError(const char *desc);
};

/// @brief No CFB file exception base
struct NoCfbFile : public CfbError {
  NoCfbFile();
};

/// @brief CFB file corrupted exception base
struct CfbFileCorrupted : public CfbError {
  CfbFileCorrupted();
};

/// @brief No text file exception
struct NoTextFile final : public std::runtime_error {
  NoTextFile();
};

/// @brief Unknown charset exception
struct UnknownCharset final : public std::runtime_error {
  UnknownCharset();
};

/// @brief No image file exception
struct NoImageFile final : public std::runtime_error {
  NoImageFile();
};

/// @brief No archive file exception
struct NoArchiveFile final : public std::runtime_error {
  NoArchiveFile();
};

/// @brief No document file exception
struct NoDocumentFile final : public std::runtime_error {
  NoDocumentFile();
};

/// @brief No open document file exception
struct NoOpenDocumentFile final : public std::runtime_error {
  NoOpenDocumentFile();
};

/// @brief No office open document file exception
struct NoOfficeOpenXmlFile final : public std::runtime_error {
  NoOfficeOpenXmlFile();
};

/// @brief No PDF file exception
struct NoPdfFile final : public std::runtime_error {
  NoPdfFile();
};

/// @brief No XML file exception
struct NoXml final : public std::runtime_error {
  NoXml();
};

/// @brief Unsupported crypto algorithm exception
struct UnsupportedCryptoAlgorithm final : public std::runtime_error {
  UnsupportedCryptoAlgorithm();
};

/// @brief No SVM file exception base
struct NoSvmFile : public std::runtime_error {
  NoSvmFile();
};

/// @brief Malformed SVM file exception base
struct MalformedSvmFile : public std::runtime_error {
  MalformedSvmFile();
};

/// @brief Unsupported endian exception
struct UnsupportedEndian final : public std::runtime_error {
  UnsupportedEndian();
};

/// @brief Unsupported MS crypto algorithm exception
struct MsUnsupportedCryptoAlgorithm final : public std::runtime_error {
  MsUnsupportedCryptoAlgorithm();
};

/// @brief Wrong password exception
struct WrongPassword final : public std::runtime_error {
  WrongPassword();
};

/// @brief Unknown document type exception
struct UnknownDocumentType final : public std::runtime_error {
  UnknownDocumentType();
};

} // namespace odr

#endif // ODR_EXCEPTIONS_HPP
