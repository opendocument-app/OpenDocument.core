#include <odr/internal/encoding/detect.hpp>

#include <odr/internal/encoding/text_encoding_table.hpp>

#include <istream>
#include <string_view>

#include <uchardet/uchardet.h>

namespace odr::internal {

namespace {

using namespace std::string_view_literals;

/// The encoding a byte-order mark names, `unknown` when there is none.
TextEncoding encoding_by_bom(const std::string_view probe) {
  const auto starts_with = [&](const std::string_view bom) {
    return probe.substr(0, bom.size()) == bom;
  };

  // utf-32le opens with the utf-16le mark, so it has to be tested first
  if (starts_with("\xff\xfe\x00\x00"sv)) {
    return TextEncoding::utf32le;
  }
  if (starts_with("\x00\x00\xfe\xff"sv)) {
    return TextEncoding::utf32be;
  }
  if (starts_with("\xef\xbb\xbf"sv)) {
    return TextEncoding::utf8;
  }
  if (starts_with("\xff\xfe"sv)) {
    return TextEncoding::utf16le;
  }
  if (starts_with("\xfe\xff"sv)) {
    return TextEncoding::utf16be;
  }
  return TextEncoding::unknown;
}

/// Whether NUL bytes are ordinary characters in @p encoding.
bool encoding_holds_nul(const TextEncoding encoding) {
  return encoding == TextEncoding::utf16le ||
         encoding == TextEncoding::utf16be ||
         encoding == TextEncoding::utf32le || encoding == TextEncoding::utf32be;
}

} // namespace

std::string encoding::read_probe(std::istream &in,
                                 const std::size_t max_bytes) {
  std::string result(max_bytes, '\0');
  in.read(result.data(), static_cast<std::streamsize>(max_bytes));
  result.resize(static_cast<std::size_t>(in.gcount()));
  return result;
}

TextEncoding encoding::detect(const std::string_view probe) {
  if (const TextEncoding by_bom = encoding_by_bom(probe);
      by_bom != TextEncoding::unknown) {
    return by_bom;
  }

  auto *const detector = uchardet_new();
  uchardet_handle_data(detector, probe.data(), probe.size());
  uchardet_data_end(detector);
  const std::string name = uchardet_get_charset(detector);
  uchardet_delete(detector);

  const auto *row = text_encoding_table::find_by_name(name);
  return row == nullptr ? TextEncoding::unknown : row->encoding;
}

bool encoding::is_text(const std::string_view probe,
                       const TextEncoding encoding) {
  // an empty file is an empty text file
  if (probe.empty()) {
    return true;
  }
  if (encoding == TextEncoding::unknown) {
    return false;
  }
  // uchardet names nul padding utf-8, so the encoding alone does not decide
  return encoding_holds_nul(encoding) ||
         probe.find('\0') == std::string_view::npos;
}

} // namespace odr::internal
