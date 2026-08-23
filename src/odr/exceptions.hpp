#pragma once

#include <cstdint>
#include <stdexcept>

namespace odr {
enum class FileType;
enum class TextEncoding;

/// @brief Base of every exception type this library declares. The decoders also
/// throw plain `std::runtime_error` for malformed input with no dedicated type,
/// so that remains the widest net.
struct Exception : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// @brief Unsupported operation exception
struct UnsupportedOperation final : Exception {
  UnsupportedOperation();
  explicit UnsupportedOperation(const std::string &message);
};

/// @brief File not found exception
struct FileNotFound final : Exception {
  FileNotFound();
  explicit FileNotFound(const std::string &path);
};

/// @brief Unknown file type exception
struct UnknownFileType final : Exception {
  UnknownFileType();
};

/// @brief Unsupported file type exception
struct UnsupportedFileType final : Exception {
  FileType file_type;

  explicit UnsupportedFileType(FileType file_type);
};

/// @brief Unsupported text encoding exception
struct UnsupportedTextEncoding final : Exception {
  TextEncoding text_encoding;

  explicit UnsupportedTextEncoding(TextEncoding text_encoding);
};

/// @brief File read error
struct FileReadError final : Exception {
  FileReadError();
};

/// @brief File write error
struct FileWriteError final : Exception {
  explicit FileWriteError(const std::string &path);
};

/// @brief No ZIP file exception base
struct NoZipFile final : Exception {
  NoZipFile();
};

/// @brief ZIP save error base; `internal::zip::MinizSaveError` refines it.
struct ZipSaveError : Exception {
  ZipSaveError();
};

/// @brief CFB error base; NoCfbFile and CfbFileCorrupted refine it.
struct CfbError : Exception {
  explicit CfbError(const std::string &desc);
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
struct NoTextFile final : Exception {
  NoTextFile();
};

/// @brief No csv file exception
struct NoCsvFile final : Exception {
  NoCsvFile();
};

/// @brief No json file exception
struct NoJsonFile final : Exception {
  NoJsonFile();
};

/// @brief Unknown charset exception
/// @deprecated Nothing throws this any more: a file whose encoding cannot be
/// named is still text, and reports @ref TextEncoding::unknown.
struct [[deprecated("nothing throws this")]] UnknownCharset final : Exception {
  UnknownCharset();
};

/// @brief No image file exception
struct NoImageFile final : Exception {
  NoImageFile();
};

/// @brief No archive file exception
struct NoArchiveFile final : Exception {
  NoArchiveFile();
};

/// @brief No document file exception
struct NoDocumentFile final : Exception {
  NoDocumentFile();
};

/// @brief No open document file exception
struct NoOpenDocumentFile final : Exception {
  NoOpenDocumentFile();
};

/// @brief No office open document file exception
struct NoOfficeOpenXmlFile final : Exception {
  NoOfficeOpenXmlFile();
};

/// @brief No PDF file exception
struct NoPdfFile final : Exception {
  NoPdfFile();
};

/// @brief No font file exception
struct NoFontFile final : Exception {
  NoFontFile();
};

/// @brief No legacy Microsoft Office file
struct NoLegacyMicrosoftFile final : Exception {
  NoLegacyMicrosoftFile();
};

/// @brief No XML file exception
struct NoXmlFile final : Exception {
  NoXmlFile();
};

/// @brief No SVG file exception
struct NoSvgFile final : Exception {
  NoSvgFile();
};

/// @brief No RTF file exception
struct NoRtfFile final : Exception {
  NoRtfFile();
};

/// @brief Unsupported crypto algorithm exception
struct UnsupportedCryptoAlgorithm final : Exception {
  UnsupportedCryptoAlgorithm();
};

/// @brief No SVM file exception base
struct NoSvmFile final : Exception {
  NoSvmFile();
};

/// @brief Malformed SVM file exception base
struct MalformedSvmFile final : Exception {
  MalformedSvmFile();
};

/// @brief Unsupported endian exception
struct UnsupportedEndian final : Exception {
  UnsupportedEndian();
};

/// @brief Unsupported MS crypto algorithm exception
struct MsUnsupportedCryptoAlgorithm final : Exception {
  MsUnsupportedCryptoAlgorithm();
};

/// @brief Unknown document type exception
struct UnknownDocumentType final : Exception {
  UnknownDocumentType();
};

/// @brief Invalid prefix string
struct InvalidPrefix final : Exception {
  InvalidPrefix();
  explicit InvalidPrefix(const std::string &prefix);
};

/// @brief Document copy protected exception
struct DocumentCopyProtectedException final : Exception {
  DocumentCopyProtectedException();
};

/// @brief Resource is not accessible
struct ResourceNotAccessible final : Exception {
  ResourceNotAccessible();
  ResourceNotAccessible(const std::string &name, const std::string &path);
};

/// @brief Prefix already in use
struct PrefixInUse final : Exception {
  PrefixInUse();
  explicit PrefixInUse(const std::string &prefix);
};

/// @brief HTTP server socket could not be bound
struct ServerBindFailed final : Exception {
  ServerBindFailed(const std::string &host, std::uint32_t port);
};

/// @brief HTTP server is bound already
struct ServerAlreadyBound final : Exception {
  ServerAlreadyBound();
};

/// @brief HTTP server has not been bound
struct ServerNotBound final : Exception {
  ServerNotBound();
};

/// @brief Unsupported option
struct UnsupportedOption final : Exception {
  explicit UnsupportedOption(const std::string &message);
};

/// @brief Null pointer error
struct NullPointerError final : Exception {
  explicit NullPointerError(const std::string &variable);
};

/// @brief Wrong password error
struct WrongPasswordError final : Exception {
  explicit WrongPasswordError();
};

/// @brief Decryption failed
struct DecryptionFailed final : Exception {
  explicit DecryptionFailed();
};

/// @brief Not encrypted error
struct NotEncryptedError final : Exception {
  explicit NotEncryptedError();
};

/// @brief Invalid path
struct InvalidPath final : Exception {
  explicit InvalidPath(const std::string &message);
};

/// @brief Unsupported file encoding
struct UnsupportedFileEncoding final : Exception {
  explicit UnsupportedFileEncoding(const std::string &message);
};

/// @brief File is encrypted
struct FileEncryptedError final : Exception {
  explicit FileEncryptedError();
};

/// @brief Read attempted on an encrypted file that has not been authenticated
struct UnauthenticatedReadError final : Exception {
  explicit UnauthenticatedReadError();
};

} // namespace odr
