#pragma once

#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/sfnt_parser.hpp>

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font::sfnt {

/// @brief `abstract::Font` over an SFNT-wrapped font — TrueType (`glyf`) or
/// OpenType/CFF (`OTTO`) — parsed from its raw bytes.
///
/// Reads the table directory and the `head`/`maxp`/`hhea`/`hmtx`/`cmap`/`name`
/// tables needed for the stage-3 facts; glyph outlines are not touched. A
/// TrueType Collection (`ttcf`) is read through its first font. Throws
/// `std::runtime_error` on a structurally invalid SFNT.
class SfntFont final : public abstract::Font {
public:
  /// Cheap magic test: a recognised SFNT version tag at the head of @p data.
  [[nodiscard]] static bool is_sfnt(std::string_view data);

  /// Takes ownership of @p in and parses the facts from it (seeking per table);
  /// the raw bytes are materialized lazily for pass-through (see `data()`).
  explicit SfntFont(std::unique_ptr<std::istream> stream);

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

  /// The font's character map: code point -> glyph index. This is the single
  /// source of truth — `glyph_for_code_point` / `code_point_for_glyph` read it,
  /// and `write()` serializes it back into the `cmap` table (every other table
  /// is copied through verbatim).
  [[nodiscard]] const std::map<char32_t, std::uint16_t> &cmap() const noexcept;

  /// Replace the character map, rebuilding the reverse (glyph -> lowest code
  /// point) lookup so the font stays self-consistent. Use this to transform the
  /// font (see `sfnt_transform.hpp`'s `reencode_to_pua`), then `write()`.
  void set_cmap(std::map<char32_t, std::uint16_t> cmap);

  /// Serialize the current state to @p out: the (possibly mutated) `cmap`
  /// rebuilt from `cmap()`, every other table copied verbatim from the source
  /// stream, with a freshly computed table directory and checksums. @p out need
  /// only be a forward sink (see `build_sfnt`).
  void write(std::ostream &out) const;

private:
  std::istream &in() const { return *m_in; }

  /// A located table within the SFNT: byte offset + length into the source.
  struct Table {
    std::uint32_t offset{};
    std::uint32_t length{};
  };
  /// Table by 4-char tag (e.g. `"cmap"`), `nullopt` if absent.
  [[nodiscard]] std::optional<Table> table(std::string_view tag) const;

  void read_directory();
  void read_head();
  void read_maxp();
  void read_hhea();
  void read_hmtx();
  void read_cmap();
  void read_name();
  void read_cmap_subtable();
  /// Rebuild m_reverse (glyph -> lowest code point) from m_cmap.
  void update_reverse();

  std::unique_ptr<std::istream> m_in;
  SfntParser m_parser;

  FontFormat m_format{FontFormat::unknown};
  std::map<std::string, Table, std::less<>> m_tables;

  std::uint16_t m_glyph_count{};
  std::uint16_t m_units_per_em{1000};
  std::uint16_t m_number_of_h_metrics{};
  FontBBox m_bbox{};
  bool m_symbolic{};

  std::vector<std::uint16_t> m_advance_widths;
  std::map<char32_t, std::uint16_t> m_cmap;
  std::map<std::uint16_t, char32_t> m_reverse; // glyph -> lowest code point
  std::string m_name;
};

} // namespace odr::internal::font::sfnt
