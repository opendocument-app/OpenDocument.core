#pragma once

#include <stdexcept>

namespace odr {
enum class FileType;
enum class DecoderEngine;

/// @brief Unsupported operation exception
struct UnsupportedOperation final : public std::runtime_error {
  UnsupportedOperation();
  explicit UnsupportedOperation(const std::string &message);
};

/// @brief File not found exception
struct FileNotFound final : public std::runtime_error {
  FileNotFound();
  explicit FileNotFound(const std::string &path);
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

/// @brief Unsupported decoder engine exception
struct UnsupportedDecoderEngine final : public std::runtime_error {
  DecoderEngine decoder_engine;

  explicit UnsupportedDecoderEngine(DecoderEngine decoder_engine);
};

/// @brief File read error
struct FileReadError final : public std::runtime_error {
  FileReadError();
};

/// @brief File write error
struct FileWriteError final : public std::runtime_error {
  explicit FileWriteError(const std::string &path);
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

/// @brief No csv file exception
struct NoCsvFile final : public std::runtime_error {
  NoCsvFile();
};

/// @brief No json file exception
struct NoJsonFile final : public std::runtime_error {
  NoJsonFile();
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

/// @brief No legacy microsoft office file
struct NoLegacyMicrosoftFile final : public std::runtime_error {
  NoLegacyMicrosoftFile();
};

/// @brief No XML file exception
struct NoXmlFile final : public std::runtime_error {
  NoXmlFile();
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

/// @brief Unknown document type exception
struct UnknownDocumentType final : public std::runtime_error {
  UnknownDocumentType();
};

/// @brief Invalid prefix string
struct InvalidPrefix final : public std::runtime_error {
  InvalidPrefix();
  explicit InvalidPrefix(const std::string &prefix);
};

/// @brief Document copy protected exception
struct DocumentCopyProtectedException : public std::runtime_error {
  DocumentCopyProtectedException();
};

/// @brief Resource is not accessible
struct ResourceNotAccessible : public std::runtime_error {
  ResourceNotAccessible();
  ResourceNotAccessible(const std::string &name, const std::string &path);
};

/// @brief Prefix already in use
struct PrefixInUse : public std::runtime_error {
  PrefixInUse();
  explicit PrefixInUse(const std::string &prefix);
};

/// @brief Unsupported option
struct UnsupportedOption : public std::runtime_error {
  explicit UnsupportedOption(const std::string &message);
};

/// @brief Null pointer error
struct NullPointerError : public std::runtime_error {
  explicit NullPointerError(const std::string &variable);
};

/// @brief Wrong password error
struct WrongPasswordError : public std::runtime_error {
  explicit WrongPasswordError();
};

/// @brief Decryption failed
struct DecryptionFailed : public std::runtime_error {
  explicit DecryptionFailed();
};

/// @brief Not encrypted error
struct NotEncryptedError : public std::runtime_error {
  explicit NotEncryptedError();
};

/// @brief Invalid path
struct InvalidPath : public std::runtime_error {
  explicit InvalidPath(const std::string &message);
};

} // namespace odr
