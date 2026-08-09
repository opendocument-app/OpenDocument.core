#include <odr/internal/encoding/transcode.hpp>

#include <odr/internal/encoding/encoding_data.hpp>
#include <odr/internal/encoding/text_encoding_table.hpp>

#include <cstdint>
#include <iterator>
#include <stdexcept>

#include <utf8cpp/utf8/checked.h>
#include <utf8cpp/utf8/cpp17.h>

namespace odr::internal {

namespace {

using encoding::encoding_data::SingleByteTable;

using namespace std::string_view_literals;

constexpr char32_t replacement = 0xfffd;

constexpr auto utf8_bom = "\xef\xbb\xbf"sv;
constexpr auto utf16le_bom = "\xff\xfe"sv;
constexpr auto utf16be_bom = "\xfe\xff"sv;
constexpr auto utf32le_bom = "\xff\xfe\x00\x00"sv;
constexpr auto utf32be_bom = "\x00\x00\xfe\xff"sv;

bool starts_with(const std::string_view bytes, const std::string_view bom) {
  return bytes.substr(0, bom.size()) == bom;
}

const SingleByteTable *single_byte_table(const TextEncoding encoding) {
  namespace data = encoding::encoding_data;

  switch (encoding) {
  case TextEncoding::ibm866:
    return &data::ibm866;
  case TextEncoding::iso_8859_1:
    return &data::iso_8859_1;
  case TextEncoding::iso_8859_2:
    return &data::iso_8859_2;
  case TextEncoding::iso_8859_3:
    return &data::iso_8859_3;
  case TextEncoding::iso_8859_4:
    return &data::iso_8859_4;
  case TextEncoding::iso_8859_5:
    return &data::iso_8859_5;
  case TextEncoding::iso_8859_6:
    return &data::iso_8859_6;
  case TextEncoding::iso_8859_7:
    return &data::iso_8859_7;
  case TextEncoding::iso_8859_8:
    return &data::iso_8859_8;
  case TextEncoding::iso_8859_10:
    return &data::iso_8859_10;
  case TextEncoding::iso_8859_13:
    return &data::iso_8859_13;
  case TextEncoding::iso_8859_14:
    return &data::iso_8859_14;
  case TextEncoding::iso_8859_15:
    return &data::iso_8859_15;
  case TextEncoding::iso_8859_16:
    return &data::iso_8859_16;
  case TextEncoding::koi8_r:
    return &data::koi8_r;
  case TextEncoding::koi8_u:
    return &data::koi8_u;
  case TextEncoding::macintosh:
    return &data::macintosh;
  case TextEncoding::windows_874:
    return &data::windows_874;
  case TextEncoding::windows_1250:
    return &data::windows_1250;
  case TextEncoding::windows_1251:
    return &data::windows_1251;
  case TextEncoding::windows_1252:
    return &data::windows_1252;
  case TextEncoding::windows_1253:
    return &data::windows_1253;
  case TextEncoding::windows_1254:
    return &data::windows_1254;
  case TextEncoding::windows_1255:
    return &data::windows_1255;
  case TextEncoding::windows_1256:
    return &data::windows_1256;
  case TextEncoding::windows_1257:
    return &data::windows_1257;
  case TextEncoding::windows_1258:
    return &data::windows_1258;
  case TextEncoding::x_mac_cyrillic:
    return &data::x_mac_cyrillic;
  default:
    return nullptr;
  }
}

/// The mark @p encoding defines, empty for the rest: `EF BB BF` read as
/// windows-1252 is three characters, not a mark.
std::string_view own_bom(const TextEncoding encoding) {
  switch (encoding) {
  case TextEncoding::utf8:
    return utf8_bom;
  case TextEncoding::utf16le:
    return utf16le_bom;
  case TextEncoding::utf16be:
    return utf16be_bom;
  case TextEncoding::utf32le:
    return utf32le_bom;
  case TextEncoding::utf32be:
    return utf32be_bom;
  default:
    return {};
  }
}

bool is_code_point_valid(const char32_t code_point) {
  return code_point <= 0x10ffff && (code_point < 0xd800 || code_point > 0xdfff);
}

std::uint8_t byte_at(const std::string_view bytes, const std::size_t index) {
  return static_cast<std::uint8_t>(bytes[index]);
}

std::string decode_single_byte(const std::string_view bytes,
                               const SingleByteTable &table) {
  std::string result;
  result.reserve(bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    utf8::append(table[byte_at(bytes, i)], std::back_inserter(result));
  }
  return result;
}

std::string decode_utf16(const std::string_view bytes, const bool little) {
  std::string result;
  result.reserve(bytes.size());

  const auto unit = [&](const std::size_t i) -> char32_t {
    const auto low = byte_at(bytes, little ? i : i + 1);
    const auto high = byte_at(bytes, little ? i + 1 : i);
    return static_cast<char32_t>(high) << 8 | low;
  };

  std::size_t i = 0;
  for (; i + 1 < bytes.size(); i += 2) {
    const char32_t first = unit(i);

    if (first < 0xd800 || first > 0xdfff) {
      utf8::append(first, std::back_inserter(result));
      continue;
    }
    // a low surrogate first, or a high one with nothing to pair with
    if (first > 0xdbff || i + 3 >= bytes.size()) {
      utf8::append(replacement, std::back_inserter(result));
      continue;
    }
    const char32_t second = unit(i + 2);
    if (second < 0xdc00 || second > 0xdfff) {
      utf8::append(replacement, std::back_inserter(result));
      continue;
    }
    utf8::append(0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00),
                 std::back_inserter(result));
    i += 2;
  }

  // a trailing odd byte cannot form a unit
  if (i < bytes.size()) {
    utf8::append(replacement, std::back_inserter(result));
  }
  return result;
}

std::string decode_utf32(const std::string_view bytes, const bool little) {
  std::string result;
  result.reserve(bytes.size() / 2);

  std::size_t i = 0;
  for (; i + 3 < bytes.size(); i += 4) {
    char32_t code_point = 0;
    for (std::size_t b = 0; b < 4; ++b) {
      code_point = code_point << 8 | byte_at(bytes, i + (little ? 3 - b : b));
    }
    utf8::append(is_code_point_valid(code_point) ? code_point : replacement,
                 std::back_inserter(result));
  }

  if (i < bytes.size()) {
    utf8::append(replacement, std::back_inserter(result));
  }
  return result;
}

} // namespace

std::string encoding::to_utf8(const std::string_view bytes,
                              const TextEncoding encoding) {
  const auto *row = text_encoding_table::find(encoding);
  if (row == nullptr || !row->decodable) {
    throw std::runtime_error("text encoding cannot be decoded");
  }

  const std::string_view bom = own_bom(encoding);
  const std::string_view body =
      starts_with(bytes, bom) ? bytes.substr(bom.size()) : bytes;

  switch (encoding) {
  case TextEncoding::utf8:
    return utf8::replace_invalid(body);
  case TextEncoding::utf16le:
    return decode_utf16(body, true);
  case TextEncoding::utf16be:
    return decode_utf16(body, false);
  case TextEncoding::utf32le:
    return decode_utf32(body, true);
  case TextEncoding::utf32be:
    return decode_utf32(body, false);
  default:
    break;
  }

  const SingleByteTable *table = single_byte_table(encoding);
  if (table == nullptr) {
    throw std::runtime_error("text encoding cannot be decoded");
  }
  return decode_single_byte(body, *table);
}

} // namespace odr::internal
