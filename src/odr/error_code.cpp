#include <odr/error_code.hpp>

#include <algorithm>
#include <array>
#include <iterator>

namespace {

struct Row final {
  odr::ErrorCode code;
  std::string_view name;
};

using odr::ErrorCode;

constexpr std::array<Row, 61> rows{{
    {ErrorCode::unknown, "Unknown"},
    {ErrorCode::unsupported_operation, "UnsupportedOperation"},
    {ErrorCode::file_not_found, "FileNotFound"},
    {ErrorCode::unknown_file_type, "UnknownFileType"},
    {ErrorCode::unsupported_file_type, "UnsupportedFileType"},
    {ErrorCode::file_read_error, "FileReadError"},
    {ErrorCode::file_write_error, "FileWriteError"},
    {ErrorCode::no_document_file, "NoDocumentFile"},
    {ErrorCode::unknown_document_type, "UnknownDocumentType"},
    {ErrorCode::unsupported_crypto_algorithm, "UnsupportedCryptoAlgorithm"},
    {ErrorCode::wrong_password, "WrongPassword"},
    {ErrorCode::decryption_failed, "DecryptionFailed"},
    {ErrorCode::not_encrypted, "NotEncrypted"},
    {ErrorCode::file_encrypted, "FileEncrypted"},
    {ErrorCode::document_copy_protected, "DocumentCopyProtected"},
    {ErrorCode::unsupported_text_encoding, "UnsupportedTextEncoding"},
    {ErrorCode::no_zip_file, "NoZipFile"},
    {ErrorCode::zip_save_error, "ZipSaveError"},
    {ErrorCode::cfb_error, "CfbError"},
    {ErrorCode::no_cfb_file, "NoCfbFile"},
    {ErrorCode::cfb_file_corrupted, "CfbFileCorrupted"},
    {ErrorCode::no_text_file, "NoTextFile"},
    {ErrorCode::no_csv_file, "NoCsvFile"},
    {ErrorCode::no_markdown_file, "NoMarkdownFile"},
    {ErrorCode::no_json_file, "NoJsonFile"},
    {ErrorCode::no_image_file, "NoImageFile"},
    {ErrorCode::no_archive_file, "NoArchiveFile"},
    {ErrorCode::no_open_document_file, "NoOpenDocumentFile"},
    {ErrorCode::no_office_open_xml_file, "NoOfficeOpenXmlFile"},
    {ErrorCode::no_pdf_file, "NoPdfFile"},
    {ErrorCode::no_font_file, "NoFontFile"},
    {ErrorCode::no_legacy_microsoft_file, "NoLegacyMicrosoftFile"},
    {ErrorCode::no_iwork_file, "NoIworkFile"},
    {ErrorCode::no_xml_file, "NoXmlFile"},
    {ErrorCode::no_svg_file, "NoSvgFile"},
    {ErrorCode::no_rtf_file, "NoRtfFile"},
    {ErrorCode::no_svm_file, "NoSvmFile"},
    {ErrorCode::malformed_svm_file, "MalformedSvmFile"},
    {ErrorCode::unsupported_endian, "UnsupportedEndian"},
    {ErrorCode::ms_unsupported_crypto_algorithm,
     "MsUnsupportedCryptoAlgorithm"},
    {ErrorCode::value_not_stated, "ValueNotStated"},
    {ErrorCode::invalid_prefix, "InvalidPrefix"},
    {ErrorCode::resource_not_accessible, "ResourceNotAccessible"},
    {ErrorCode::prefix_in_use, "PrefixInUse"},
    {ErrorCode::server_bind_failed, "ServerBindFailed"},
    {ErrorCode::server_already_bound, "ServerAlreadyBound"},
    {ErrorCode::server_not_bound, "ServerNotBound"},
    {ErrorCode::unsupported_option, "UnsupportedOption"},
    {ErrorCode::null_pointer_error, "NullPointerError"},
    {ErrorCode::invalid_path, "InvalidPath"},
    {ErrorCode::unsupported_file_encoding, "UnsupportedFileEncoding"},
    {ErrorCode::unauthenticated_read_error, "UnauthenticatedReadError"},

    {ErrorCode::edit_new_line, "newLine"},
    {ErrorCode::edit_formula, "formula"},
    {ErrorCode::edit_rich, "rich"},
    {ErrorCode::edit_shapes, "shapes"},
    {ErrorCode::edit_read_only, "readOnly"},
    {ErrorCode::edit_formula_input, "formulaInput"},
    {ErrorCode::edit_unsupported, "unsupportedEdit"},
    {ErrorCode::edit_range, "range"},
    {ErrorCode::edit_unnameable, "unnameableEdit"},
}};

} // namespace

std::string_view odr::error_code_name(const ErrorCode code) noexcept {
  const auto row = std::ranges::find(rows, code, &Row::code);
  return row == rows.end() ? std::string_view{"Unknown"} : row->name;
}

std::vector<odr::ErrorCode> odr::all_error_codes() {
  std::vector<ErrorCode> result;
  result.reserve(rows.size());
  std::ranges::transform(rows, std::back_inserter(result), &Row::code);
  return result;
}
