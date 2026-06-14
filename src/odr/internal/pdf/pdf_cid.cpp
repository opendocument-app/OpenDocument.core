#include <odr/internal/pdf/pdf_cid.hpp>

#include <odr/internal/util/string_util.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace odr::internal::pdf {

namespace {

// The Adobe predefined Unicode CMaps name their flavour in the middle segment:
// `Uni<Collection>-UCS2-H`, `-UTF16-H`, `-UTF32-H` (optionally with a
// `HW`/`Pro` or writing-mode suffix). UCS-2 and UTF-16 are both 2-byte
// big-endian code units (UTF-16 additionally pairs surrogates); UTF-32 is
// 4-byte big-endian.
enum class UnicodeCodec { utf16be, utf32be };

std::optional<UnicodeCodec> classify(const std::string_view name) {
  if (name.find("UTF32") != std::string_view::npos) {
    return UnicodeCodec::utf32be;
  }
  if (name.find("UTF16") != std::string_view::npos ||
      name.find("UCS2") != std::string_view::npos) {
    return UnicodeCodec::utf16be;
  }
  return std::nullopt;
}

std::uint8_t byte(const std::string &codes, const std::size_t i) {
  return static_cast<std::uint8_t>(codes[i]);
}

std::string decode_utf16be(const std::string &codes) {
  std::u16string units;
  units.reserve(codes.size() / 2);
  for (std::size_t i = 0; i + 1 < codes.size(); i += 2) {
    units.push_back(
        static_cast<char16_t>((byte(codes, i) << 8) | byte(codes, i + 1)));
  }
  return util::string::u16string_to_string(units);
}

std::string decode_utf32be(const std::string &codes) {
  std::string result;
  for (std::size_t i = 0; i + 3 < codes.size(); i += 4) {
    const auto codepoint = static_cast<char32_t>(
        (byte(codes, i) << 24) | (byte(codes, i + 1) << 16) |
        (byte(codes, i + 2) << 8) | byte(codes, i + 3));
    util::string::append_c32(codepoint, result);
  }
  return result;
}

} // namespace

std::optional<std::string>
translate_predefined_cmap(const std::string_view name,
                          const std::string &codes) {
  const std::optional<UnicodeCodec> codec = classify(name);
  if (!codec.has_value()) {
    return std::nullopt;
  }
  return *codec == UnicodeCodec::utf32be ? decode_utf32be(codes)
                                         : decode_utf16be(codes);
}

} // namespace odr::internal::pdf
