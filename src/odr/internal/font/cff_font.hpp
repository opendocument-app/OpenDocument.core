#pragma once

#include <odr/internal/abstract/font.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font::cff {

/// @brief `abstract::Font` over a **bare CFF** font program — a PDF
/// `/FontFile3` (`/Subtype /Type1C` or `/CIDFontType0C`) or the `CFF ` table
/// lifted out of an OpenType font.
///
/// Per the "IR for facts, pass-through for glyphs" architecture this parses the
/// CFF structure (INDEXes, DICTs, charset, Private DICT) for the facts every
/// consumer needs but never interprets glyph charstrings as outlines — the raw
/// CFF bytes are kept for pass-through (`data()`), to be embedded verbatim as a
/// `CFF ` table when wrapping into an OTF. Reads CFF format 1
/// (Adobe TN #5176); CFF2 is out of scope.
///
/// Throws `std::runtime_error` on a structurally invalid CFF.
class CffFont final : public abstract::Font {
public:
  /// Cheap magic test: a CFF header (major version 1, sane `hdrSize`).
  [[nodiscard]] static bool is_cff(std::string_view data);

  /// Parse the facts from an in-memory CFF blob (retained for `data()`).
  explicit CffFont(std::string data);

  [[nodiscard]] FontFormat format() const noexcept override;
  [[nodiscard]] std::string name() const override;
  [[nodiscard]] std::uint16_t glyph_count() const noexcept override;
  [[nodiscard]] std::uint16_t units_per_em() const noexcept override;
  [[nodiscard]] bool symbolic() const noexcept override;
  [[nodiscard]] FontBBox bounding_box() const noexcept override;
  [[nodiscard]] std::uint16_t advance_width(std::uint16_t glyph) const override;
  [[nodiscard]] std::uint16_t
  glyph_for_code_point(char32_t code_point) const override;
  [[nodiscard]] std::optional<char32_t>
  code_point_for_glyph(std::uint16_t glyph) const override;
  [[nodiscard]] std::uint16_t
  glyph_for_name(std::string_view name) const override;

  // --- CFF-specific facts, for the OTF wrap and PDF wiring ---

  /// Whether the font is CID-keyed (the Top DICT carries `ROS`). A CID-keyed
  /// CFF's charset maps glyph -> CID rather than glyph -> name.
  [[nodiscard]] bool is_cid_keyed() const noexcept;

  /// The glyph's PostScript name (non-CID fonts), empty when unresolved (a
  /// CID-keyed font, an out-of-range glyph, or an unknown SID).
  [[nodiscard]] std::string glyph_name(std::uint16_t glyph) const;

  /// charset glyph -> CID (CID-keyed fonts), `0` when out of range or not
  /// CID-keyed.
  [[nodiscard]] std::uint16_t cid_for_glyph(std::uint16_t glyph) const;
  /// charset CID -> glyph (CID-keyed fonts), `0` (`.notdef`) when unmapped.
  [[nodiscard]] std::uint16_t glyph_for_cid(std::uint16_t cid) const;

  /// The raw CFF bytes, for verbatim pass-through into a `CFF ` table.
  [[nodiscard]] std::string_view data() const noexcept;

private:
  /// A byte range [offset, offset+length) into `m_data`.
  struct Range {
    std::uint32_t offset{};
    std::uint32_t length{};
  };

  /// The two Private DICT entries a charstring's width is resolved against
  /// (Adobe TN #5177 "width").
  struct Widths {
    double default_width{};
    double nominal_width{};
  };

  void parse();
  void parse_top_dict(Range top_dict);
  [[nodiscard]] Widths parse_private_dict(Range private_dict) const;
  /// Parse `/FDArray`'s per-FD Private DICTs and `/FDSelect` (CID-keyed fonts;
  /// Adobe TN #5176 §19). Without these a CID font resolves every width against
  /// a `nominalWidthX` of 0.
  void parse_fd_array(std::uint32_t offset);
  void parse_fd_select(std::uint32_t offset);
  /// The Private DICT widths governing @p glyph — its FD's for a CID-keyed
  /// font, the Top DICT's otherwise.
  [[nodiscard]] const Widths &widths_for_glyph(std::uint16_t glyph) const;
  void parse_charset(std::uint32_t offset);
  /// Materialize a predefined charset (id 0 ISOAdobe / 1 Expert / 2
  /// ExpertSubset) into `m_charset`, used when `/charset` is a predefined id or
  /// omitted (Adobe TN #5176 §13, Appendix C).
  void load_predefined_charset(std::uint32_t id);

  /// Parse a CFF INDEX starting at @p offset, returning one `Range` per member
  /// (into `m_data`); @p end receives the offset just past the INDEX.
  [[nodiscard]] std::vector<Range> read_index(std::uint32_t offset,
                                              std::uint32_t &end) const;
  /// Resolve a string SID to its text (standard strings or the String INDEX).
  [[nodiscard]] std::string string_for_sid(std::uint16_t sid) const;
  /// The inverse, `nullopt` when the font never names @p string.
  [[nodiscard]] std::optional<std::uint16_t>
  sid_for_string(std::string_view string) const;
  /// Extract the optional leading width from glyph @p glyph's Type2 charstring,
  /// in design units; `nullopt` when the charstring carries no explicit width.
  [[nodiscard]] std::optional<std::int32_t>
  charstring_width(std::uint16_t glyph) const;

  std::string m_data;

  bool m_cid_keyed{};
  std::uint16_t m_units_per_em{1000};
  FontBBox m_bbox{};
  std::string m_name;

  std::vector<Range> m_charstrings;     // per-glyph charstring byte ranges
  std::vector<Range> m_strings;         // String INDEX members (SID 391+)
  std::vector<std::uint16_t> m_charset; // glyph -> SID (or CID, CID-keyed)

  Widths m_widths;                       // Top DICT Private DICT
  std::vector<Widths> m_fd_widths;       // per FDArray entry, CID-keyed only
  std::vector<std::uint8_t> m_fd_select; // glyph -> FDArray index
};

} // namespace odr::internal::font::cff
