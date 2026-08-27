#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::pdf {

/// A parsed CMap: character codes (1+ big-endian bytes, width from the
/// codespace ranges) to Unicode (several UTF-16 units for a ligature) and/or to
/// CIDs. `translate_string` splits an input string and concatenates the
/// destinations.
class CMap {
public:
  void add_codespace_range(std::string low_code, std::string high_code);
  void map_single(std::string code, std::u16string unicode);

  /// CID mapping from a composite font's `/Encoding` CMap stream (ISO 32000-1
  /// 9.7.5.3); a range maps `base_cid + (code - low)`. Codes are keyed by their
  /// raw bytes *and* width, so `<20>` and `<0020>` stay distinct.
  void map_cid_char(std::string code, std::uint32_t cid);
  void add_cid_range(std::uint32_t low, std::uint32_t high,
                     std::uint32_t base_cid, std::size_t width);

  /// True when no code -> Unicode mapping was parsed (e.g. the font carries no
  /// `ToUnicode` CMap); the caller then falls back to the `/Encoding`.
  [[nodiscard]] bool empty() const { return m_map.empty(); }

  /// Records that the CMap stream referenced another CMap via `usecmap` (ISO
  /// 32000-1 9.7.5.3). We do not resolve the inherited base, so whatever
  /// codespace this stream declares locally is (potentially) incomplete and is
  /// no longer treated as authoritative (see `has_codespace`).
  void mark_inherits_external_cmap() { m_inherits_external_cmap = true; }

  /// True when this stream referenced an unresolved base CMap via `usecmap`.
  [[nodiscard]] bool inherits_external_cmap() const {
    return m_inherits_external_cmap;
  }

  /// True when this CMap's `code_width` may be trusted to split a code string:
  /// it declares at least one range and inherits no unresolved base CMap. A
  /// `usecmap` stream is excluded because its local ranges may cover only an
  /// override subset. False sends callers to another codespace or a fixed
  /// width.
  [[nodiscard]] bool has_codespace() const {
    return !m_codespace_ranges.empty() && !m_inherits_external_cmap;
  }

  /// Byte width of a code starting with `first`, from the codespace ranges
  /// (matched on the first byte, ISO 32000-1 9.7.6.2); 1 when none matches.
  /// Public so the glyph/advance paths split exactly as `translate_string`
  /// does, keeping a mixed 1-/2-byte codespace aligned across both.
  [[nodiscard]] std::size_t code_width(std::uint8_t first) const;

  /// `single_byte_codes` overrides the codespace ranges and splits the codes
  /// one byte each (a simple font's width, ISO 32000-1 9.10.3). An imposed
  /// single-byte code is also looked up zero-padded to two bytes, producers
  /// keying the entries either way.
  [[nodiscard]] std::string
  translate_string(const std::string &codes,
                   bool single_byte_codes = false) const;

  /// True when at least one `cidchar`/`cidrange` mapping was parsed (an
  /// embedded CID `/Encoding` CMap). When false the composite code -> CID is
  /// identity
  /// (`Identity-H/V`).
  [[nodiscard]] bool has_cid_map() const {
    return !m_cid_chars.empty() || !m_cid_ranges.empty();
  }

  /// The CID a code (raw big-endian bytes) selects, or `nullopt` when no
  /// `cidchar`/`cidrange` covers it; the caller then falls back to identity
  /// (CID = code). The width of `code` is matched, so a 1-byte code never hits
  /// a 2-byte range.
  [[nodiscard]] std::optional<std::uint32_t>
  cid_for_code(std::string_view code) const;

private:
  struct CodespaceRange {
    // `low` and `high` share the same length; that length is the code width in
    // bytes for codes that start within this range.
    std::string low;
    std::string high;
  };

  struct CidRange {
    std::uint32_t low;      ///< numeric value of the low code
    std::uint32_t high;     ///< numeric value of the high code
    std::uint32_t base_cid; ///< CID of the low code
    std::size_t width; ///< byte width of the codes (to disambiguate widths)
  };

  bool m_inherits_external_cmap{false};
  std::vector<CodespaceRange> m_codespace_ranges;
  std::unordered_map<std::string, std::u16string> m_map;
  std::unordered_map<std::string, std::uint32_t> m_cid_chars;
  std::vector<CidRange> m_cid_ranges;

  /// Byte width of the code starting at `pos`, decided by the codespace ranges;
  /// falls back to a single byte when no range declares/matches it.
  [[nodiscard]] std::size_t code_length(const std::string &codes,
                                        std::size_t pos) const;
};

} // namespace odr::internal::pdf
