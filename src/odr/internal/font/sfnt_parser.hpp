#pragma once

#include <odr/internal/abstract/font.hpp>

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font::sfnt {

// SFNT enumerations (OpenType spec). Values are the on-disk codes; casting a
// raw `u16` to one and switching/comparing keeps the magic numbers in one
// place.

/// `cmap`/`name` platform IDs.
enum class PlatformId : std::uint16_t {
  unicode = 0,
  macintosh = 1,
  windows = 3,
};

/// Windows-platform (`PlatformId::windows`) encoding IDs.
enum class WindowsEncoding : std::uint16_t {
  symbol = 0,
  unicode_bmp = 1,
  unicode_full = 10,
};

/// Unicode-platform (`PlatformId::unicode`) encoding IDs that name a full
/// (beyond-BMP) repertoire; the rest are treated as plain Unicode.
enum class UnicodeEncoding : std::uint16_t {
  unicode_2_0_full = 4,
  unicode_full = 6,
};

/// Macintosh-platform (`PlatformId::macintosh`) encoding IDs.
enum class MacintoshEncoding : std::uint16_t {
  roman = 0,
};

/// `cmap` subtable format (the leading `u16` of a subtable).
enum class CmapFormat : std::uint16_t {
  byte_encoding = 0,      // format 0
  segment_mapping = 4,    // format 4
  trimmed_mapping = 6,    // format 6
  segmented_coverage = 12 // format 12
};

/// `name` record IDs we extract a font name from.
enum class NameId : std::uint16_t {
  full = 4,
  postscript = 6,
};

struct CmapEntry {
  PlatformId platform;
  std::uint16_t encoding;
  std::uint32_t offset;
};

struct NameEntry {
  PlatformId platform;
  NameId name_id;
  std::uint16_t name_length;
  std::uint16_t name_local_offset;

  [[nodiscard]] bool utf16() const {
    return platform == PlatformId::windows || platform == PlatformId::unicode;
  }
};

class SfntParser final {
public:
  /// Cheap magic test: a recognised SFNT version tag at the head of @p data.
  [[nodiscard]] static bool is_sfnt(std::string_view data);

  explicit SfntParser(std::istream &stream);

  std::istream &in() const { return *m_in; }

  void seek(std::streampos offset);
  [[nodiscard]] std::streampos tell();
  void ignore(std::streamsize count);

  [[nodiscard]] std::uint8_t read_u8();
  [[nodiscard]] std::uint16_t read_u16();
  [[nodiscard]] std::uint32_t read_u32();
  [[nodiscard]] std::string read_tag();
  /// Decode a UTF-16BE `name` string (Windows/Unicode platforms), surrogate
  /// pairs included, into UTF-8.
  [[nodiscard]] std::string read_utf16be(std::size_t len);
  /// Decode a single-byte (Mac/Latin-1) `name` string into UTF-8.
  [[nodiscard]] std::string read_latin1(std::size_t len);
  [[nodiscard]] FontBBox read_bbox();
  [[nodiscard]] CmapEntry read_cmap_entry();
  [[nodiscard]] NameEntry read_name_entry();

private:
  std::istream *m_in{};
};

} // namespace odr::internal::font::sfnt
