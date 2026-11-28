#pragma once

#include <odr/style.hpp>

#include <odr/internal/oldms/text/doc_structs.hpp>

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::oldms::text {

/// Owns the document's resolved styles, indexed by the style indices stored on
/// paragraph and span elements, 0 being the default, and the page layout.
/// Immutable after construction.
class StyleRegistry final {
public:
  StyleRegistry() = default;
  StyleRegistry(std::vector<TextStyle> text_styles,
                std::vector<ParagraphStyle> paragraph_styles,
                const PageLayout &page_layout);

  /// Throws if the index has no style.
  [[nodiscard]] const TextStyle &text_style(std::uint32_t index) const;
  /// Throws if the index has no style.
  [[nodiscard]] const ParagraphStyle &
  paragraph_style(std::uint32_t index) const;
  [[nodiscard]] const PageLayout &page_layout() const;

private:
  std::vector<TextStyle> m_text_styles;
  std::vector<ParagraphStyle> m_paragraph_styles;
  PageLayout m_page_layout;
};

/// The paragraph styles of a style sheet (STSH, [MS-DOC] 2.9.271), each
/// resolved through its `istdBase` chain. Immutable after construction.
class StyleSheet final {
public:
  struct Entry {
    ParagraphStyle paragraph;
    TextStyle text;
  };

  /// `base` holds the properties beneath every style; `entries` is indexed by
  /// istd, unset where the istd names no paragraph style.
  StyleSheet(Entry base, std::vector<std::optional<Entry>> entries);

  /// The style named by `istd`; Normal (istd 0) where it names none, and
  /// `base` where Normal is missing too ([MS-DOC] 2.4.6.5).
  [[nodiscard]] const Entry &paragraph_style(std::uint16_t istd) const;

private:
  Entry m_base;
  std::vector<std::optional<Entry>> m_entries;
};

/// The base character style Chpx grpprls apply on top of; without sprmCHps
/// the font size defaults to 20 half-points ([MS-DOC] 2.6.1 sprmCHps).
[[nodiscard]] TextStyle default_character_style();

/// The page without a Sepx: the defaults of the section SPRMs ([MS-DOC]
/// 2.6.4), a US Letter page with 1.25in sides and 1in top and bottom.
[[nodiscard]] PageLayout default_page_layout();

/// Applies the character SPRMs of a grpprl ([MS-DOC] 2.6.1) on top of
/// `style`, against which a ToggleOperand resolves; other SPRMs are skipped.
/// `font_names` resolves sprmCRgFtc0.
TextStyle apply_character_sprms(TextStyle style, std::string_view grpprl,
                                std::span<const std::string> font_names);

/// Applies the paragraph SPRMs of a grpprl ([MS-DOC] 2.6.2) on top of
/// `style`; other SPRMs are skipped.
ParagraphStyle apply_paragraph_sprms(ParagraphStyle style,
                                     std::string_view grpprl);

/// Applies the section SPRMs of a grpprl ([MS-DOC] 2.6.4) on top of
/// `layout`; other SPRMs are skipped.
PageLayout apply_section_sprms(PageLayout layout, std::string_view grpprl);

/// Reads the font names (FFN.xszFfn) of the SttbfFfn ([MS-DOC] 2.9.286) at
/// `fc` in the table stream; empty for lcb == 0.
std::vector<std::string> read_font_names(std::istream &table_stream,
                                         FcLcb sttbf_ffn);

/// Reads the paragraph styles of the STSH at `stshf` in the table stream, on
/// top of `default_character_style` and the style sheet's default font.
StyleSheet read_style_sheet(std::istream &table_stream, FcLcb stshf,
                            std::span<const std::string> font_names);

/// The page layout of the first section: its Sepx ([MS-DOC] 2.9.253), located
/// through the PlcfSed at `plcf_sed` in the table stream, applied to
/// `default_page_layout`.
PageLayout read_page_layout(std::istream &document_stream,
                            std::istream &table_stream, FcLcb plcf_sed);

} // namespace odr::internal::oldms::text
