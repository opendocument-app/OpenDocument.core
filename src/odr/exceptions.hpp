#pragma once

#include <cstdint>
#include <stdexcept>

namespace odr {
enum class FileType;
enum class TextEncoding;

/// Base of every exception type this library declares. The decoders also throw
/// plain `std::runtime_error` for malformed input with no dedicated type, so
/// that remains the widest net.
struct Exception : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// Unsupported operation exception
struct UnsupportedOperation final : Exception {
  UnsupportedOperation();
  explicit UnsupportedOperation(const std::string &message);
};

/// File not found exception
struct FileNotFound final : Exception {
  FileNotFound();
  explicit FileNotFound(const std::string &path);
};

/// Unknown file type exception
struct UnknownFileType final : Exception {
  UnknownFileType();
};

/// Unsupported file type exception
struct UnsupportedFileType final : Exception {
  FileType file_type;

  explicit UnsupportedFileType(FileType file_type);
};

/// Unsupported text encoding exception
struct UnsupportedTextEncoding final : Exception {
  TextEncoding text_encoding;

  explicit UnsupportedTextEncoding(TextEncoding text_encoding);
};

/// File read error
struct FileReadError final : Exception {
  FileReadError();
};

/// File write error
struct FileWriteError final : Exception {
  explicit FileWriteError(const std::string &path);
};

/// No ZIP file exception base
struct NoZipFile final : Exception {
  NoZipFile();
};

/// ZIP save error base; `internal::zip::MinizSaveError` refines it.
struct ZipSaveError : Exception {
  ZipSaveError();
};

/// CFB error base; NoCfbFile and CfbFileCorrupted refine it.
struct CfbError : Exception {
  explicit CfbError(const std::string &desc);
};

/// No CFB file exception base
struct NoCfbFile final : CfbError {
  NoCfbFile();
};

/// CFB file corrupted exception base
struct CfbFileCorrupted final : CfbError {
  CfbFileCorrupted();
};

/// No text file exception
struct NoTextFile final : Exception {
  NoTextFile();
};

/// No csv file exception
struct NoCsvFile final : Exception {
  NoCsvFile();
};

/// No markdown file exception
struct NoMarkdownFile final : Exception {
  NoMarkdownFile();
};

/// No json file exception
struct NoJsonFile final : Exception {
  NoJsonFile();
};

/// No image file exception
struct NoImageFile final : Exception {
  NoImageFile();
};

/// No archive file exception
struct NoArchiveFile final : Exception {
  NoArchiveFile();
};

/// No document file exception
struct NoDocumentFile final : Exception {
  NoDocumentFile();
};

/// No open document file exception
struct NoOpenDocumentFile final : Exception {
  NoOpenDocumentFile();
};

/// No office open document file exception
struct NoOfficeOpenXmlFile final : Exception {
  NoOfficeOpenXmlFile();
};

/// No PDF file exception
struct NoPdfFile final : Exception {
  NoPdfFile();
};

/// No font file exception
struct NoFontFile final : Exception {
  NoFontFile();
};

/// No legacy Microsoft Office file
struct NoLegacyMicrosoftFile final : Exception {
  NoLegacyMicrosoftFile();
};

/// No iWork file exception
struct NoIworkFile final : Exception {
  NoIworkFile();
};

/// No XML file exception
struct NoXmlFile final : Exception {
  NoXmlFile();
};

/// No SVG file exception
struct NoSvgFile final : Exception {
  NoSvgFile();
};

/// No RTF file exception
struct NoRtfFile final : Exception {
  NoRtfFile();
};

/// Unsupported crypto algorithm exception
struct UnsupportedCryptoAlgorithm final : Exception {
  UnsupportedCryptoAlgorithm();
};

/// No SVM file exception base
struct NoSvmFile final : Exception {
  NoSvmFile();
};

/// Malformed SVM file exception base
struct MalformedSvmFile final : Exception {
  MalformedSvmFile();
};

/// Unsupported endian exception
struct UnsupportedEndian final : Exception {
  UnsupportedEndian();
};

/// Unsupported MS crypto algorithm exception
struct MsUnsupportedCryptoAlgorithm final : Exception {
  MsUnsupportedCryptoAlgorithm();
};

/// Unknown document type exception
struct UnknownDocumentType final : Exception {
  UnknownDocumentType();
};

/// Invalid prefix string
struct InvalidPrefix final : Exception {
  InvalidPrefix();
  explicit InvalidPrefix(const std::string &prefix);
};

/// Document copy protected exception
struct DocumentCopyProtectedException final : Exception {
  DocumentCopyProtectedException();
};

/// Resource is not accessible
struct ResourceNotAccessible final : Exception {
  ResourceNotAccessible();
  ResourceNotAccessible(const std::string &name, const std::string &path);
};

/// Prefix already in use
struct PrefixInUse final : Exception {
  PrefixInUse();
  explicit PrefixInUse(const std::string &prefix);
};

/// HTTP server socket could not be bound
struct ServerBindFailed final : Exception {
  ServerBindFailed(const std::string &host, std::uint32_t port);
};

/// HTTP server is bound already
struct ServerAlreadyBound final : Exception {
  ServerAlreadyBound();
};

/// HTTP server has not been bound
struct ServerNotBound final : Exception {
  ServerNotBound();
};

/// Unsupported option
struct UnsupportedOption final : Exception {
  explicit UnsupportedOption(const std::string &message);
};

/// Null pointer error
struct NullPointerError final : Exception {
  explicit NullPointerError(const std::string &variable);
};

/// Wrong password error
struct WrongPasswordError final : Exception {
  explicit WrongPasswordError();
};

/// Decryption failed
struct DecryptionFailed final : Exception {
  explicit DecryptionFailed();
};

/// Not encrypted error
struct NotEncryptedError final : Exception {
  explicit NotEncryptedError();
};

/// Invalid path
struct InvalidPath final : Exception {
  explicit InvalidPath(const std::string &message);
};

/// Unsupported file encoding
struct UnsupportedFileEncoding final : Exception {
  explicit UnsupportedFileEncoding(const std::string &message);
};

/// File is encrypted
struct FileEncryptedError final : Exception {
  explicit FileEncryptedError();
};

/// Read attempted on an encrypted file that has not been authenticated
struct UnauthenticatedReadError final : Exception {
  explicit UnauthenticatedReadError();
};

} // namespace odr
