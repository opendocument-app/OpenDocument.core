#include <odr/internal/encoding/text_encoding_table.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace odr::internal::encoding {

namespace {

using text_encoding_table::Row;

using namespace std::string_view_literals;

// Names per encoding, canonical one first — it goes into an html
// `<meta charset>`, so it has to be a label a browser knows. Aliases are unique
// across encodings; `odr_test` asserts no name appears twice.

constexpr std::array utf8_names{"UTF-8"sv, "ASCII"sv, "US-ASCII"sv};
// a bare `UTF-16`/`UTF-32` only reaches here without a mark, where little
// endian is the likelier intent
constexpr std::array utf16le_names{"UTF-16LE"sv, "UTF-16"sv};
constexpr std::array utf16be_names{"UTF-16BE"sv};
constexpr std::array utf32le_names{"UTF-32LE"sv, "UTF-32"sv};
constexpr std::array utf32be_names{"UTF-32BE"sv};

constexpr std::array ibm866_names{"IBM866"sv, "cp866"sv, "866"sv};
constexpr std::array iso_8859_1_names{"ISO-8859-1"sv, "latin1"sv, "l1"sv,
                                      "iso-ir-100"sv, "cp819"sv};
constexpr std::array iso_8859_2_names{"ISO-8859-2"sv, "latin2"sv, "l2"sv};
constexpr std::array iso_8859_3_names{"ISO-8859-3"sv, "latin3"sv, "l3"sv};
constexpr std::array iso_8859_4_names{"ISO-8859-4"sv, "latin4"sv, "l4"sv};
constexpr std::array iso_8859_5_names{"ISO-8859-5"sv, "cyrillic"sv};
constexpr std::array iso_8859_6_names{"ISO-8859-6"sv, "arabic"sv};
constexpr std::array iso_8859_7_names{"ISO-8859-7"sv, "greek"sv};
constexpr std::array iso_8859_8_names{"ISO-8859-8"sv, "hebrew"sv};
constexpr std::array iso_8859_10_names{"ISO-8859-10"sv, "latin6"sv, "l6"sv};
constexpr std::array iso_8859_13_names{"ISO-8859-13"sv};
constexpr std::array iso_8859_14_names{"ISO-8859-14"sv, "latin8"sv, "l8"sv};
constexpr std::array iso_8859_15_names{"ISO-8859-15"sv, "latin9"sv, "l9"sv};
constexpr std::array iso_8859_16_names{"ISO-8859-16"sv, "latin10"sv, "l10"sv};
constexpr std::array koi8_r_names{"KOI8-R"sv, "cskoi8r"sv};
constexpr std::array koi8_u_names{"KOI8-U"sv};
constexpr std::array macintosh_names{"macintosh"sv, "mac"sv, "x-mac-roman"sv,
                                     "macroman"sv};
constexpr std::array windows_874_names{"windows-874"sv, "cp874"sv, "TIS-620"sv,
                                       "ISO-8859-11"sv};
constexpr std::array windows_1250_names{"windows-1250"sv, "cp1250"sv,
                                        "x-cp1250"sv};
constexpr std::array windows_1251_names{"windows-1251"sv, "cp1251"sv,
                                        "x-cp1251"sv};
constexpr std::array windows_1252_names{"windows-1252"sv, "cp1252"sv,
                                        "x-cp1252"sv, "ansi"sv};
constexpr std::array windows_1253_names{"windows-1253"sv, "cp1253"sv};
constexpr std::array windows_1254_names{"windows-1254"sv, "cp1254"sv};
constexpr std::array windows_1255_names{"windows-1255"sv, "cp1255"sv};
constexpr std::array windows_1256_names{"windows-1256"sv, "cp1256"sv};
constexpr std::array windows_1257_names{"windows-1257"sv, "cp1257"sv};
constexpr std::array windows_1258_names{"windows-1258"sv, "cp1258"sv};
constexpr std::array x_mac_cyrillic_names{"x-mac-cyrillic"sv, "mac-cyrillic"sv};

constexpr std::array big5_names{"Big5"sv, "csbig5"sv, "big5-hkscs"sv,
                                "cn-big5"sv};
constexpr std::array euc_jp_names{"EUC-JP"sv, "cseucpkdfmtjapanese"sv};
constexpr std::array euc_kr_names{"EUC-KR"sv, "windows-949"sv, "cp949"sv,
                                  "ks_c_5601-1987"sv};
constexpr std::array gb18030_names{"gb18030"sv, "GBK"sv, "GB2312"sv,
                                   "csgb2312"sv, "x-gbk"sv};
constexpr std::array iso_2022_jp_names{"ISO-2022-JP"sv, "csiso2022jp"sv};
constexpr std::array iso_2022_kr_names{"ISO-2022-KR"sv, "csiso2022kr"sv};
constexpr std::array shift_jis_names{"Shift_JIS"sv, "sjis"sv, "windows-31j"sv,
                                     "cp932"sv, "ms_kanji"sv};

constexpr std::array table{
    Row{TextEncoding::utf8, utf8_names, true},
    Row{TextEncoding::utf16le, utf16le_names, true},
    Row{TextEncoding::utf16be, utf16be_names, true},
    Row{TextEncoding::utf32le, utf32le_names, true},
    Row{TextEncoding::utf32be, utf32be_names, true},

    Row{TextEncoding::ibm866, ibm866_names, true},
    Row{TextEncoding::iso_8859_1, iso_8859_1_names, true},
    Row{TextEncoding::iso_8859_2, iso_8859_2_names, true},
    Row{TextEncoding::iso_8859_3, iso_8859_3_names, true},
    Row{TextEncoding::iso_8859_4, iso_8859_4_names, true},
    Row{TextEncoding::iso_8859_5, iso_8859_5_names, true},
    Row{TextEncoding::iso_8859_6, iso_8859_6_names, true},
    Row{TextEncoding::iso_8859_7, iso_8859_7_names, true},
    Row{TextEncoding::iso_8859_8, iso_8859_8_names, true},
    Row{TextEncoding::iso_8859_10, iso_8859_10_names, true},
    Row{TextEncoding::iso_8859_13, iso_8859_13_names, true},
    Row{TextEncoding::iso_8859_14, iso_8859_14_names, true},
    Row{TextEncoding::iso_8859_15, iso_8859_15_names, true},
    Row{TextEncoding::iso_8859_16, iso_8859_16_names, true},
    Row{TextEncoding::koi8_r, koi8_r_names, true},
    Row{TextEncoding::koi8_u, koi8_u_names, true},
    Row{TextEncoding::macintosh, macintosh_names, true},
    Row{TextEncoding::windows_874, windows_874_names, true},
    Row{TextEncoding::windows_1250, windows_1250_names, true},
    Row{TextEncoding::windows_1251, windows_1251_names, true},
    Row{TextEncoding::windows_1252, windows_1252_names, true},
    Row{TextEncoding::windows_1253, windows_1253_names, true},
    Row{TextEncoding::windows_1254, windows_1254_names, true},
    Row{TextEncoding::windows_1255, windows_1255_names, true},
    Row{TextEncoding::windows_1256, windows_1256_names, true},
    Row{TextEncoding::windows_1257, windows_1257_names, true},
    Row{TextEncoding::windows_1258, windows_1258_names, true},
    Row{TextEncoding::x_mac_cyrillic, x_mac_cyrillic_names, true},

    // decoding these needs a real converter; naming them still lets a browser
    // do it
    Row{TextEncoding::big5, big5_names, false},
    Row{TextEncoding::euc_jp, euc_jp_names, false},
    Row{TextEncoding::euc_kr, euc_kr_names, false},
    Row{TextEncoding::gb18030, gb18030_names, false},
    Row{TextEncoding::iso_2022_jp, iso_2022_jp_names, false},
    Row{TextEncoding::iso_2022_kr, iso_2022_kr_names, false},
    Row{TextEncoding::shift_jis, shift_jis_names, false},
};

/// Case, `-`, `_` and spaces carry no meaning in an encoding label.
std::string normalize(const std::string_view name) {
  std::string result;
  result.reserve(name.size());
  for (const char c : name) {
    if (c == '-' || c == '_' || c == ' ') {
      continue;
    }
    result.push_back(util::string::to_lower(c));
  }
  return result;
}

} // namespace

std::span<const text_encoding_table::Row> text_encoding_table::rows() noexcept {
  return table;
}

const text_encoding_table::Row *
text_encoding_table::find(const TextEncoding encoding) noexcept {
  const auto it = std::ranges::find(table, encoding, &Row::encoding);
  return it == std::ranges::end(table) ? nullptr : &*it;
}

const text_encoding_table::Row *
text_encoding_table::find_by_name(const std::string_view name) noexcept {
  const std::string needle = normalize(name);
  const auto it = std::ranges::find_if(table, [&](const Row &row) {
    return std::ranges::any_of(row.names, [&](const std::string_view alias) {
      return normalize(alias) == needle;
    });
  });
  return it == std::ranges::end(table) ? nullptr : &*it;
}

} // namespace odr::internal::encoding
