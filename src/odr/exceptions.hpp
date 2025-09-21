#pragma once

#include <stdexcept>

namespace odr {
enum class FileType;
enum class DecoderEngine;

/// @brief Unsupported operation exception
struct UnsupportedOperation final : std::runtime_error {
  UnsupportedOperation();
  explicit UnsupportedOperation(const std::string &message);
};

/// @brief File not found exception
struct FileNotFound final : std::runtime_error {
  FileNotFound();
  explicit FileNotFound(const std::string &path);
};

/// @brief Unknown file type exception
struct UnknownFileType final : std::runtime_error {
  UnknownFileType();
};

/// @brief Unsupported file type exception
struct UnsupportedFileType final : std::runtime_error {
  FileType file_type;

  explicit UnsupportedFileType(FileType file_type);
};

/// @brief Unknown decoder engine exception
struct UnknownDecoderEngine final : std::runtime_error {
  UnknownDecoderEngine();
};

/// @brief Unsupported decoder engine exception
struct UnsupportedDecoderEngine final : std::runtime_error {
  DecoderEngine decoder_engine;

  explicit UnsupportedDecoderEngine(DecoderEngine decoder_engine);
};

/// @brief File read error
struct FileReadError final : std::runtime_error {
  FileReadError();
};

/// @brief File write error
struct FileWriteError final : std::runtime_error {
  explicit FileWriteError(const std::string &path);
};

/// @brief No ZIP file exception base
struct NoZipFile final : std::runtime_error {
  NoZipFile();
};

/// @brief ZIP save error base
struct ZipSaveError : std::runtime_error {
  ZipSaveError();
};

/// @brief CFB error base
struct CfbError : std::runtime_error {
  explicit CfbError(const char *desc);
};

/// @brief No CFB file exception base
struct NoCfbFile final : CfbError {
  NoCfbFile();
};

/// @brief CFB file corrupted exception base
struct CfbFileCorrupted final : CfbError {
  CfbFileCorrupted();
};

/// @brief No text file exception
struct NoTextFile final : std::runtime_error {
  NoTextFile();
};

/// @brief No csv file exception
struct NoCsvFile final : std::runtime_error {
  NoCsvFile();
};

/// @brief No json file exception
struct NoJsonFile final : std::runtime_error {
  NoJsonFile();
};

/// @brief Unknown charset exception
struct UnknownCharset final : std::runtime_error {
  UnknownCharset();
};

/// @brief No image file exception
struct NoImageFile final : std::runtime_error {
  NoImageFile();
};

/// @brief No archive file exception
struct NoArchiveFile final : std::runtime_error {
  NoArchiveFile();
};

/// @brief No document file exception
struct NoDocumentFile final : std::runtime_error {
  NoDocumentFile();
};

/// @brief No open document file exception
struct NoOpenDocumentFile final : std::runtime_error {
  NoOpenDocumentFile();
};

/// @brief No office open document file exception
struct NoOfficeOpenXmlFile final : std::runtime_error {
  NoOfficeOpenXmlFile();
};

/// @brief No PDF file exception
struct NoPdfFile final : std::runtime_error {
  NoPdfFile();
};

/// @brief No legacy Microsoft Office file
struct NoLegacyMicrosoftFile final : std::runtime_error {
  NoLegacyMicrosoftFile();
};

/// @brief No XML file exception
struct NoXmlFile final : std::runtime_error {
  NoXmlFile();
};

/// @brief Unsupported crypto algorithm exception
struct UnsupportedCryptoAlgorithm final : std::runtime_error {
  UnsupportedCryptoAlgorithm();
};

/// @brief No SVM file exception base
struct NoSvmFile final : std::runtime_error {
  NoSvmFile();
};

/// @brief Malformed SVM file exception base
struct MalformedSvmFile final : std::runtime_error {
  MalformedSvmFile();
};

/// @brief Unsupported endian exception
struct UnsupportedEndian final : std::runtime_error {
  UnsupportedEndian();
};

/// @brief Unsupported MS crypto algorithm exception
struct MsUnsupportedCryptoAlgorithm final : std::runtime_error {
  MsUnsupportedCryptoAlgorithm();
};

/// @brief Unknown document type exception
struct UnknownDocumentType final : std::runtime_error {
  UnknownDocumentType();
};

/// @brief Invalid prefix string
struct InvalidPrefix final : std::runtime_error {
  InvalidPrefix();
  explicit InvalidPrefix(const std::string &prefix);
};

/// @brief Document copy protected exception
struct DocumentCopyProtectedException final : std::runtime_error {
  DocumentCopyProtectedException();
};

/// @brief Resource is not accessible
struct ResourceNotAccessible final : std::runtime_error {
  ResourceNotAccessible();
  ResourceNotAccessible(const std::string &name, const std::string &path);
};

/// @brief Prefix already in use
struct PrefixInUse final : std::runtime_error {
  PrefixInUse();
  explicit PrefixInUse(const std::string &prefix);
};

/// @brief Unsupported option
struct UnsupportedOption final : std::runtime_error {
  explicit UnsupportedOption(const std::string &message);
};

/// @brief Null pointer error
struct NullPointerError final : std::runtime_error {
  explicit NullPointerError(const std::string &variable);
};

/// @brief Wrong password error
struct WrongPasswordError final : std::runtime_error {
  explicit WrongPasswordError();
};

/// @brief Decryption failed
struct DecryptionFailed final : std::runtime_error {
  explicit DecryptionFailed();
};

/// @brief Not encrypted error
struct NotEncryptedError final : std::runtime_error {
  explicit NotEncryptedError();
};

/// @brief Invalid path
struct InvalidPath final : std::runtime_error {
  explicit InvalidPath(const std::string &message);
};

} // namespace odr
