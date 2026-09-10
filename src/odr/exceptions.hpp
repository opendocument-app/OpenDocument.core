#pragma once

#include <odr/error_code.hpp>

#include <cstdint>
#include <exception>
#include <stdexcept>

namespace odr {
enum class FileType;
enum class TextEncoding;

/// Base of every exception type this library declares. The decoders also throw
/// plain `std::runtime_error` for malformed input with no dedicated type, so
/// that remains the widest net.
struct Exception : std::runtime_error {
  using std::runtime_error::runtime_error;

  /// What a binding reports. @ref ErrorCode::unknown where a type states none.
  [[nodiscard]] virtual ErrorCode code() const noexcept {
    return ErrorCode::unknown;
  }
};

/// Base for an exception type whose code is @p C.
template <ErrorCode C> struct CodedException : Exception {
  using Exception::Exception;

  [[nodiscard]] ErrorCode code() const noexcept override { return C; }
};

/// @ref ErrorCode of @p exception, @ref ErrorCode::unknown for anything that is
/// not an @ref Exception.
[[nodiscard]] ErrorCode error_code(const std::exception &exception) noexcept;

/// Unsupported operation exception
struct UnsupportedOperation final
    : CodedException<ErrorCode::unsupported_operation> {
  UnsupportedOperation();
  explicit UnsupportedOperation(const std::string &message);
};

/// File not found exception
struct FileNotFound final : CodedException<ErrorCode::file_not_found> {
  FileNotFound();
  explicit FileNotFound(const std::string &path);
};

/// Unknown file type exception
struct UnknownFileType final : CodedException<ErrorCode::unknown_file_type> {
  UnknownFileType();
};

/// Unsupported file type exception
struct UnsupportedFileType final
    : CodedException<ErrorCode::unsupported_file_type> {
  FileType file_type;

  explicit UnsupportedFileType(FileType file_type);
};

/// Unsupported text encoding exception
struct UnsupportedTextEncoding final
    : CodedException<ErrorCode::unsupported_text_encoding> {
  TextEncoding text_encoding;

  explicit UnsupportedTextEncoding(TextEncoding text_encoding);
};

/// File read error
struct FileReadError final : CodedException<ErrorCode::file_read_error> {
  FileReadError();
};

/// File write error
struct FileWriteError final : CodedException<ErrorCode::file_write_error> {
  explicit FileWriteError(const std::string &path);
};

/// No ZIP file exception base
struct NoZipFile final : CodedException<ErrorCode::no_zip_file> {
  NoZipFile();
};

/// ZIP save error base; `internal::zip::MinizSaveError` refines it.
struct ZipSaveError : CodedException<ErrorCode::zip_save_error> {
  ZipSaveError();
};

/// CFB error base; NoCfbFile and CfbFileCorrupted refine it.
struct CfbError : CodedException<ErrorCode::cfb_error> {
  explicit CfbError(const std::string &desc);
};

/// No CFB file exception base
struct NoCfbFile final : CfbError {
  NoCfbFile();

  [[nodiscard]] ErrorCode code() const noexcept override {
    return ErrorCode::no_cfb_file;
  }
};

/// CFB file corrupted exception base
struct CfbFileCorrupted final : CfbError {
  CfbFileCorrupted();

  [[nodiscard]] ErrorCode code() const noexcept override {
    return ErrorCode::cfb_file_corrupted;
  }
};

/// No text file exception
struct NoTextFile final : CodedException<ErrorCode::no_text_file> {
  NoTextFile();
};

/// No csv file exception
struct NoCsvFile final : CodedException<ErrorCode::no_csv_file> {
  NoCsvFile();
};

/// No markdown file exception
struct NoMarkdownFile final : CodedException<ErrorCode::no_markdown_file> {
  NoMarkdownFile();
};

/// No json file exception
struct NoJsonFile final : CodedException<ErrorCode::no_json_file> {
  NoJsonFile();
};

/// No image file exception
struct NoImageFile final : CodedException<ErrorCode::no_image_file> {
  NoImageFile();
};

/// No archive file exception
struct NoArchiveFile final : CodedException<ErrorCode::no_archive_file> {
  NoArchiveFile();
};

/// No document file exception
struct NoDocumentFile final : CodedException<ErrorCode::no_document_file> {
  NoDocumentFile();
};

/// No open document file exception
struct NoOpenDocumentFile final
    : CodedException<ErrorCode::no_open_document_file> {
  NoOpenDocumentFile();
};

/// No office open document file exception
struct NoOfficeOpenXmlFile final
    : CodedException<ErrorCode::no_office_open_xml_file> {
  NoOfficeOpenXmlFile();
};

/// No PDF file exception
struct NoPdfFile final : CodedException<ErrorCode::no_pdf_file> {
  NoPdfFile();
};

/// No font file exception
struct NoFontFile final : CodedException<ErrorCode::no_font_file> {
  NoFontFile();
};

/// No legacy Microsoft Office file
struct NoLegacyMicrosoftFile final
    : CodedException<ErrorCode::no_legacy_microsoft_file> {
  NoLegacyMicrosoftFile();
};

/// No iWork file exception
struct NoIworkFile final : CodedException<ErrorCode::no_iwork_file> {
  NoIworkFile();
};

/// No XML file exception
struct NoXmlFile final : CodedException<ErrorCode::no_xml_file> {
  NoXmlFile();
};

/// No SVG file exception
struct NoSvgFile final : CodedException<ErrorCode::no_svg_file> {
  NoSvgFile();
};

/// No RTF file exception
struct NoRtfFile final : CodedException<ErrorCode::no_rtf_file> {
  NoRtfFile();
};

/// Unsupported crypto algorithm exception
struct UnsupportedCryptoAlgorithm final
    : CodedException<ErrorCode::unsupported_crypto_algorithm> {
  UnsupportedCryptoAlgorithm();
};

/// No SVM file exception base
struct NoSvmFile final : CodedException<ErrorCode::no_svm_file> {
  NoSvmFile();
};

/// Malformed SVM file exception base
struct MalformedSvmFile final : CodedException<ErrorCode::malformed_svm_file> {
  MalformedSvmFile();
};

/// Unsupported endian exception
struct UnsupportedEndian final : CodedException<ErrorCode::unsupported_endian> {
  UnsupportedEndian();
};

/// Unsupported MS crypto algorithm exception
struct MsUnsupportedCryptoAlgorithm final
    : CodedException<ErrorCode::ms_unsupported_crypto_algorithm> {
  MsUnsupportedCryptoAlgorithm();
};

/// Unknown document type exception
struct UnknownDocumentType final
    : CodedException<ErrorCode::unknown_document_type> {
  UnknownDocumentType();
};

/// A value asked of something that states none, e.g. `CellValue::number` on a
/// cell holding a string
struct ValueNotStated final : CodedException<ErrorCode::value_not_stated> {
  ValueNotStated();
};

/// Invalid prefix string
struct InvalidPrefix final : CodedException<ErrorCode::invalid_prefix> {
  InvalidPrefix();
  explicit InvalidPrefix(const std::string &prefix);
};

/// Document copy protected exception
struct DocumentCopyProtectedException final
    : CodedException<ErrorCode::document_copy_protected> {
  DocumentCopyProtectedException();
};

/// Resource is not accessible
struct ResourceNotAccessible final
    : CodedException<ErrorCode::resource_not_accessible> {
  ResourceNotAccessible();
  ResourceNotAccessible(const std::string &name, const std::string &path);
};

/// Prefix already in use
struct PrefixInUse final : CodedException<ErrorCode::prefix_in_use> {
  PrefixInUse();
  explicit PrefixInUse(const std::string &prefix);
};

/// HTTP server socket could not be bound
struct ServerBindFailed final : CodedException<ErrorCode::server_bind_failed> {
  ServerBindFailed(const std::string &host, std::uint32_t port);
};

/// HTTP server is bound already
struct ServerAlreadyBound final
    : CodedException<ErrorCode::server_already_bound> {
  ServerAlreadyBound();
};

/// HTTP server has not been bound
struct ServerNotBound final : CodedException<ErrorCode::server_not_bound> {
  ServerNotBound();
};

/// Unsupported option
struct UnsupportedOption final : CodedException<ErrorCode::unsupported_option> {
  explicit UnsupportedOption(const std::string &message);
};

/// Null pointer error
struct NullPointerError final : CodedException<ErrorCode::null_pointer_error> {
  explicit NullPointerError(const std::string &variable);
};

/// Wrong password error
struct WrongPasswordError final : CodedException<ErrorCode::wrong_password> {
  explicit WrongPasswordError();
};

/// Decryption failed
struct DecryptionFailed final : CodedException<ErrorCode::decryption_failed> {
  explicit DecryptionFailed();
};

/// Not encrypted error
struct NotEncryptedError final : CodedException<ErrorCode::not_encrypted> {
  explicit NotEncryptedError();
};

/// Invalid path
struct InvalidPath final : CodedException<ErrorCode::invalid_path> {
  explicit InvalidPath(const std::string &message);
};

/// Unsupported file encoding
struct UnsupportedFileEncoding final
    : CodedException<ErrorCode::unsupported_file_encoding> {
  explicit UnsupportedFileEncoding(const std::string &message);
};

/// File is encrypted
struct FileEncryptedError final : CodedException<ErrorCode::file_encrypted> {
  explicit FileEncryptedError();
};

/// Read attempted on an encrypted file that has not been authenticated
struct UnauthenticatedReadError final
    : CodedException<ErrorCode::unauthenticated_read_error> {
  explicit UnauthenticatedReadError();
};

} // namespace odr
