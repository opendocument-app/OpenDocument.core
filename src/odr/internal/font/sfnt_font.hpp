#pragma once

#include <odr/internal/abstract/font.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::font {

/// @brief `FontProgram` over an SFNT-wrapped font — TrueType (`glyf`) or
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

  explicit SfntFont(std::string data);

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
  [[nodiscard]] const std::string &data() const noexcept override;

  /// A located table within the SFNT: byte offset + length into `data()`.
  struct Table {
    std::uint32_t offset{};
    std::uint32_t length{};
  };
  /// Table by 4-char tag (e.g. `"cmap"`), `nullopt` if absent. Exposed for the
  /// OTF writer (3.1).
  [[nodiscard]] std::optional<Table> table(std::string_view tag) const;

private:
  void parse_directory(std::uint32_t sfnt_offset);
  void parse_head();
  void parse_maxp();
  void parse_hhea();
  void parse_hmtx();
  void parse_cmap();
  void parse_name();
  void parse_cmap_subtable(std::uint32_t offset);

  std::string m_data;
  FontFormat m_format{FontFormat::unknown};
  std::map<std::string, Table, std::less<>> m_tables;

  std::uint16_t m_glyph_count{};
  std::uint16_t m_units_per_em{1000};
  std::uint16_t m_number_of_h_metrics{};
  FontBBox m_bbox{};
  bool m_symbolic{};

  std::vector<std::uint16_t> m_advance_widths;
  std::map<char32_t, std::uint16_t> m_cmap;
  std::map<std::uint16_t, char32_t> m_reverse;
  std::string m_name;
};

} // namespace odr::internal::font
