#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace odr::internal::oldms::spreadsheet {

// NOTE: like the sibling .doc/.ppt modules, multi-byte values are read in host
// byte order, so this is correct only on little-endian hosts.

/// Sequential reader over the flat BIFF8 record stream ([MS-XLS] 2.1.4).
///
/// `next_record` advances to the next record header (skipping any unread body
/// bytes). The `read_*` body accessors hop transparently into a following
/// CONTINUE record when the current body is exhausted — required for the SST
/// and String records, whose payload may be split across CONTINUE records —
/// and throw if more bytes are requested but the next record is not a
/// CONTINUE.
class BiffReader final {
public:
  explicit BiffReader(std::istream &in);

  /// Advances to the next record header.
  /// @return false at end of stream.
  bool next_record();
  /// Seeks to an absolute stream offset (a BoundSheet8.lbPlyPos) so the next
  /// `next_record` reads the record header at that offset.
  void seek(std::uint32_t offset);

  [[nodiscard]] std::uint16_t record_type() const;
  /// Unread bytes in the current record body (excluding CONTINUE spill).
  [[nodiscard]] std::size_t remaining() const;

  std::uint8_t read_u8();
  std::uint16_t read_u16();
  std::uint32_t read_u32();
  void read_bytes(void *out, std::size_t count);
  template <typename T> void read(T &out) { read_bytes(&out, sizeof(T)); }
  template <typename T> T read() {
    T value;
    read(value);
    return value;
  }
  void skip_bytes(std::size_t count);

  /// Reads `cch` characters of an [MS-XLS] Unicode string body: 2 bytes per
  /// character (UTF-16LE) if `high_byte`, else 1 byte (code points U+0000 to
  /// U+00FF). At each CONTINUE boundary inside the character data a fresh
  /// flags byte re-declares `high_byte` for the remainder ([MS-XLS] 2.5.293).
  /// @return the string converted to UTF-8.
  std::string read_unicode_chars(std::size_t cch, bool high_byte);

  /// Reads a ShortXLUnicodeString ([MS-XLS] 2.5.240): u8 cch, u8 flags, chars.
  /// Used for the BoundSheet8 sheet name.
  std::string read_short_xl_unicode_string();

  /// Reads an XLUnicodeString ([MS-XLS] 2.5.294): u16 cch, u8 flags, chars.
  /// Used for the String and Label record bodies.
  std::string read_xl_unicode_string();

  /// Reads an XLUnicodeRichExtendedString ([MS-XLS] 2.5.293), dropping the
  /// formatting runs and phonetic data. Used for the SST string array.
  std::string read_xl_unicode_rich_extended_string();

  void expect_bof();

private:
  std::istream *m_in{nullptr};
  std::uint16_t m_record_type{0};
  std::size_t m_remaining{0};

  /// Hops into the following CONTINUE record; throws if it is anything else.
  void next_continue();
};

/// Decodes an RkNumber ([MS-XLS] 2.5.217): bit 0 = fX100 (divide by 100),
/// bit 1 = fInt (30-bit signed integer vs. high 30 bits of an IEEE double).
double decode_rk(std::uint32_t rk);

/// Formats a numeric cell value roughly like Excel's "General" format
/// (up to 15 significant digits, no trailing zeros).
std::string format_number(double value);

/// Maps a BErr error code ([MS-XLS] 2.5.10) to its display string.
std::string error_code_string(std::uint8_t error);

} // namespace odr::internal::oldms::spreadsheet
