#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::pdf {

/// Maps PDF character codes to Unicode, as described by a `ToUnicode` CMap.
///
/// A code is a sequence of bytes (1 or more, big-endian) whose width is defined
/// by the codespace ranges; a destination is a sequence of UTF-16 units (more
/// than one for ligatures). `translate_string` splits an input string into
/// codes of the right width and concatenates their destinations.
class CMap {
public:
  void add_codespace_range(std::string low_code, std::string high_code);
  void map_single(std::string code, std::u16string unicode);

  /// CID mapping (a composite font's `/Encoding` CMap stream, ISO 32000-1
  /// 9.7.5.3): `cidchar` maps a single code, `cidrange` maps a contiguous block
  /// (`base_cid + (code - low)`). Codes are keyed by their raw big-endian bytes
  /// / width so a 1-byte and a 2-byte code with the same numeric value stay
  /// distinct (`<20>` -> CID 229 vs `<0020>` -> CID 32), the mixed-width case.
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

  /// True when this CMap declares an authoritative codespace: at least one
  /// range, and it does not inherit an unresolved base CMap via `usecmap`. When
  /// true, the code widths this CMap implies are authoritative for splitting a
  /// code string (see `code_width`); when false, callers fall back to another
  /// CMap's codespace or a fixed width. An inherited (`usecmap`) codespace is
  /// deliberately excluded — its local ranges may cover only an override
  /// subset, so trusting them would mis-split the inherited (e.g. 2-byte)
  /// codes.
  [[nodiscard]] bool has_codespace() const {
    return !m_codespace_ranges.empty() && !m_inherits_external_cmap;
  }

  /// Byte width of a code whose first byte is `first`, decided by the codespace
  /// ranges (matched on the first byte, ISO 32000-1 9.7.6.2). Falls back to a
  /// single byte when no range declares/matches it. This is the variable-width
  /// split that `translate_string` uses; exposing it lets the glyph/advance
  /// paths split codes identically, so a mixed 1-/2-byte codespace (e.g. a
  /// 1-byte space among 2-byte CIDs) stays aligned across both.
  [[nodiscard]] std::size_t code_width(std::uint8_t first) const;

  [[nodiscard]] std::string translate_string(const std::string &codes) const;

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
    std::uint32_t low;      // numeric value of the low code
    std::uint32_t high;     // numeric value of the high code
    std::uint32_t base_cid; // CID of the low code
    std::size_t width;      // byte width of the codes (to disambiguate widths)
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
