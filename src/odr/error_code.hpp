#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace odr {

/// @brief What went wrong, as one number every binding reports.
///
/// Below 1000 an @ref Exception names itself, 1 to 15 in the order `ODRError`
/// shipped. From 1000 the rendered page names an edit it refused, which
/// nothing throws. **Appended, never renumbered.** Every code needs a row in
/// `error_code.cpp`.
enum class ErrorCode : std::int32_t {
  unknown = 1, ///< Also any `std::exception` that is not an @ref Exception.
  unsupported_operation = 2,
  file_not_found = 3,
  unknown_file_type = 4,
  unsupported_file_type = 5,
  file_read_error = 6,
  file_write_error = 7,
  no_document_file = 8,
  unknown_document_type = 9,
  unsupported_crypto_algorithm = 10,
  wrong_password = 11,
  decryption_failed = 12,
  not_encrypted = 13,
  file_encrypted = 14,
  document_copy_protected = 15,

  unsupported_text_encoding = 16,
  no_zip_file = 17,
  zip_save_error = 18,
  cfb_error = 19,
  no_cfb_file = 20,
  cfb_file_corrupted = 21,
  no_text_file = 22,
  no_csv_file = 23,
  no_markdown_file = 24,
  no_json_file = 25,
  no_image_file = 26,
  no_archive_file = 27,
  no_open_document_file = 28,
  no_office_open_xml_file = 29,
  no_pdf_file = 30,
  no_font_file = 31,
  no_legacy_microsoft_file = 32,
  no_iwork_file = 33,
  no_xml_file = 34,
  no_svg_file = 35,
  no_rtf_file = 36,
  no_svm_file = 37,
  malformed_svm_file = 38,
  unsupported_endian = 39,
  ms_unsupported_crypto_algorithm = 40,
  value_not_stated = 41,
  invalid_prefix = 42,
  resource_not_accessible = 43,
  prefix_in_use = 44,
  server_bind_failed = 45,
  server_already_bound = 46,
  server_not_bound = 47,
  unsupported_option = 48,
  null_pointer_error = 49,
  invalid_path = 50,
  unsupported_file_encoding = 51,
  unauthenticated_read_error = 52,

  /// The editing scripts report these through `odr.onEditRefused` and
  /// `odr.onError`.
  edit_new_line = 1001, ///< A line break inside a paragraph.
  edit_formula = 1002,
  edit_rich = 1003, ///< The cell holds more than one plain run.
  edit_shapes = 1004,
  edit_read_only = 1005,
  edit_formula_input = 1006,
  edit_unsupported = 1007,
  edit_range = 1008,      ///< The range reaches over a picture or a table.
  edit_unnameable = 1009, ///< The edit landed where no operation names it.
};

/// @brief The code's name, as the bindings already spell it.
///
/// An identifier, not a message: nothing here is localised, so a host maps the
/// code to its own wording.
[[nodiscard]] std::string_view error_code_name(ErrorCode code) noexcept;

/// Every code, in declaration order.
[[nodiscard]] std::vector<ErrorCode> all_error_codes();

} // namespace odr
