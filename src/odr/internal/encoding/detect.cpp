#include <odr/internal/encoding/detect.hpp>

#include <odr/internal/encoding/text_encoding_table.hpp>
#include <odr/internal/util/stream_util.hpp>

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

/// Whether @p probe holds non-ascii bytes and all of them form well formed
/// utf-8 sequences. A sequence the probe's end cuts off counts as well formed,
/// since the probe is a prefix of the file.
bool is_utf8_beyond_ascii(const std::string_view probe) {
  bool beyond_ascii = false;
  for (std::size_t i = 0; i < probe.size();) {
    const auto lead = static_cast<unsigned char>(probe[i]);
    if (lead < 0x80) {
      ++i;
      continue;
    }
    beyond_ascii = true;
    std::size_t length = 0;
    // the bounds of the second byte rule out overlong forms, surrogates and
    // code points past U+10FFFF
    unsigned char low = 0x80;
    unsigned char high = 0xbf;
    if (lead >= 0xc2 && lead <= 0xdf) {
      length = 2;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      length = 3;
      low = lead == 0xe0 ? 0xa0 : 0x80;
      high = lead == 0xed ? 0x9f : 0xbf;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      length = 4;
      low = lead == 0xf0 ? 0x90 : 0x80;
      high = lead == 0xf4 ? 0x8f : 0xbf;
    } else {
      return false;
    }
    for (std::size_t k = 1; k < length; ++k) {
      if (i + k >= probe.size()) {
        return true;
      }
      const auto next = static_cast<unsigned char>(probe[i + k]);
      if (next < (k == 1 ? low : 0x80) || next > (k == 1 ? high : 0xbf)) {
        return false;
      }
    }
    i += length;
  }
  return beyond_ascii;
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
  return util::stream::read(in, max_bytes);
}

TextEncoding encoding::detect(const std::string_view probe) {
  if (const TextEncoding by_bom = encoding_by_bom(probe);
      by_bom != TextEncoding::unknown) {
    return by_bom;
  }
  // uchardet weighs a few non-ascii characters among much ascii by
  // single-byte statistics and names such utf-8 MAC-CENTRALEUROPE, IBM852 or
  // ISO-8859-11; text in a legacy single-byte encoding is well formed utf-8
  // only by rare accident
  if (is_utf8_beyond_ascii(probe)) {
    return TextEncoding::utf8;
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
