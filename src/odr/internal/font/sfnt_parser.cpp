#include <odr/internal/font/sfnt_parser.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <istream>

namespace odr::internal::font::sfnt {

namespace {

void append_utf8(std::string &out, const char32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xc0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xe0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  }
}

} // namespace

bool SfntParser::is_sfnt(const std::string_view data) {
  if (data.size() < 4) {
    return false;
  }
  const std::string_view tag = data.substr(0, 4);
  return tag == std::string_view("\x00\x01\x00\x00", 4) || tag == "OTTO" ||
         tag == "true" || tag == "ttcf" || tag == "typ1";
}

SfntParser::SfntParser(std::istream &in) : m_in{&in} {}

void SfntParser::seek(const std::streampos offset) { in().seekg(offset); }

std::streampos SfntParser::tell() { return in().tellg(); }

void SfntParser::ignore(const std::streamsize count) { in().ignore(count); }

std::uint8_t SfntParser::read_u8() { return util::byte_stream::read_u8(in()); }

std::uint16_t SfntParser::read_u16() {
  return util::byte_stream::read_u16_be(in());
}

std::uint32_t SfntParser::read_u32() {
  return util::byte_stream::read_u32_be(in());
}

std::string SfntParser::read_tag() { return util::stream::read(in(), 4); }

std::string SfntParser::read_utf16be(const std::size_t len) {
  std::string out;
  for (std::size_t i = 0; i + 1 < len; i += 2) {
    char32_t cp = read_u16();
    if (cp >= 0xd800 && cp <= 0xdbff && i + 3 < len) {
      const char32_t lo = read_u16();
      if (lo >= 0xdc00 && lo <= 0xdfff) {
        cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
        i += 2;
      }
    }
    append_utf8(out, cp);
  }
  return out;
}

std::string SfntParser::read_latin1(const std::size_t len) {
  std::string out;
  for (std::size_t i = 0; i < len; ++i) {
    append_utf8(out, read_u8());
  }
  return out;
}

FontBBox SfntParser::read_bbox() {
  const std::int16_t x_min = read_u16();
  const std::int16_t y_min = read_u16();
  const std::int16_t x_max = read_u16();
  const std::int16_t y_max = read_u16();
  return {x_min, y_min, x_max, y_max};
}

CmapEntry SfntParser::read_cmap_entry() {
  const auto platform = static_cast<PlatformId>(read_u16());
  const std::uint16_t encoding = read_u16();
  const std::uint32_t offset = read_u32();
  return {platform, encoding, offset};
}

NameEntry SfntParser::read_name_entry() {
  const auto platform = static_cast<PlatformId>(read_u16());
  ignore(4); // encodingID, languageID
  const auto name_id = static_cast<NameId>(read_u16());
  const std::uint16_t length = read_u16();
  const std::uint16_t local_offset = read_u16();
  return {platform, name_id, length, local_offset};
}

} // namespace odr::internal::font::sfnt
