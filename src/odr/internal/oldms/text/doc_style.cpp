#include <odr/internal/oldms/text/doc_style.hpp>

#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/internal/util/byte_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <istream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace odr::internal::oldms::text {

namespace {

/// The Ico palette ([MS-DOC] 2.9.119); index 0 is cvAuto (no explicit color).
constexpr std::array<std::uint32_t, 17> ico_colors = {
    0x000000, 0x000000, 0x0000FF, 0x00FFFF, 0x00FF00, 0xFF00FF,
    0xFF0000, 0xFFFF00, 0xFFFFFF, 0x000080, 0x008080, 0x008000,
    0x800080, 0x800000, 0x808000, 0x808080, 0xC0C0C0};

std::optional<Color> ico_color(const std::uint8_t ico) {
  if (ico >= ico_colors.size()) {
    throw std::runtime_error("doc: Ico value out of range");
  }
  if (ico == 0) {
    return std::nullopt; // cvAuto
  }
  return Color::from_rgb(ico_colors[ico]);
}

/// ToggleOperand ([MS-DOC] 2.9.327): 0x80 takes the style's value, 0x81 its
/// opposite.
bool toggle_on(const std::uint8_t value, const bool style_value) {
  switch (value) {
  case 0x00:
    return false;
  case 0x01:
    return true;
  case 0x80:
    return style_value;
  case 0x81:
    return !style_value;
  default:
    throw std::runtime_error("doc: unexpected ToggleOperand value");
  }
}

Measure twips(const double value) {
  return Measure(value / 1440.0, DynamicUnit("in"));
}

std::int16_t operand_i16(const std::string_view operand) {
  return static_cast<std::int16_t>(
      util::byte::from_little_endian<std::uint16_t>(operand));
}

/// Calls `visit(opcode, operand)` for each Prl of a grpprl ([MS-DOC] 2.2.5.1,
/// 2.2.5.2). A variable-length operand is given without its size prefix.
template <typename Visit>
void for_each_prl(const std::string_view grpprl, const Visit &visit) {
  const auto byte_at = [&grpprl](const std::size_t at) {
    if (at >= grpprl.size()) {
      throw std::runtime_error("doc: Prl operand overruns grpprl");
    }
    return static_cast<std::uint8_t>(grpprl[at]);
  };

  std::size_t at = 0;
  while (at + sizeof(Sprm) <= grpprl.size()) {
    Sprm sprm;
    std::memcpy(&sprm, grpprl.data() + at, sizeof(sprm));
    const std::uint16_t opcode = std::bit_cast<std::uint16_t>(sprm);
    at += sizeof(sprm);

    std::size_t operand_size;
    if (const std::int32_t fixed_size = sprm.operand_size(); fixed_size >= 0) {
      operand_size = static_cast<std::size_t>(fixed_size);
    } else if (opcode == sprmTDefTable) {
      // A u16 size, counting one more byte than follows it.
      const std::size_t cb = byte_at(at) | byte_at(at + 1) << 8;
      if (cb == 0) {
        throw std::runtime_error("doc: sprmTDefTable without a size");
      }
      at += 2;
      operand_size = cb - 1;
    } else {
      operand_size = byte_at(at);
      ++at;
      if (opcode == sprmPChgTabs && operand_size == 0xFF) {
        // PChgTabsOperand ([MS-DOC] 2.9.182): tabs deleted at four bytes
        // each, then tabs added at three.
        const std::size_t deleted = byte_at(at);
        const std::size_t added = byte_at(at + 1 + deleted * 4);
        operand_size = 2 + deleted * 4 + added * 3;
      }
    }
    if (at + operand_size > grpprl.size()) {
      throw std::runtime_error("doc: Prl operand overruns grpprl");
    }
    visit(opcode, grpprl.substr(at, operand_size));
    at += operand_size;
  }
  if (at != grpprl.size()) {
    throw std::runtime_error("doc: grpprl does not hold whole Prls");
  }
}

} // namespace

StyleRegistry::StyleRegistry(std::vector<TextStyle> text_styles,
                             std::vector<ParagraphStyle> paragraph_styles,
                             const PageLayout &page_layout)
    : m_text_styles(std::move(text_styles)),
      m_paragraph_styles(std::move(paragraph_styles)),
      m_page_layout(page_layout) {}

const TextStyle &StyleRegistry::text_style(const std::uint32_t index) const {
  return m_text_styles.at(index);
}

const ParagraphStyle &
StyleRegistry::paragraph_style(const std::uint32_t index) const {
  return m_paragraph_styles.at(index);
}

const PageLayout &StyleRegistry::page_layout() const { return m_page_layout; }

StyleSheet::StyleSheet(Entry base, std::vector<std::optional<Entry>> entries)
    : m_base(std::move(base)), m_entries(std::move(entries)) {}

const StyleSheet::Entry &
StyleSheet::paragraph_style(const std::uint16_t istd) const {
  for (const std::uint16_t candidate : {istd, std::uint16_t{0}}) {
    if (candidate >= m_entries.size()) {
      continue;
    }
    if (const std::optional<Entry> &entry = m_entries[candidate];
        entry.has_value()) {
      return *entry;
    }
  }
  return m_base;
}

TextStyle default_character_style() {
  TextStyle style;
  style.font_size = Measure(10.0, DynamicUnit("pt"));
  return style;
}

PageLayout default_page_layout() {
  PageLayout layout;
  layout.width = twips(12240);
  layout.height = twips(15840);
  layout.margin.left = twips(1800);
  layout.margin.right = twips(1800);
  layout.margin.top = twips(1440);
  layout.margin.bottom = twips(1440);
  return layout;
}

} // namespace odr::internal::oldms::text

namespace odr::internal::oldms {

TextStyle
text::apply_character_sprms(TextStyle style, const std::string_view grpprl,
                            const std::span<const std::string> font_names) {
  const TextStyle style_value = style;

  for_each_prl(grpprl, [&](const std::uint16_t opcode,
                           const std::string_view operand) {
    switch (opcode) {
    case sprmCFBold:
      style.font_weight = toggle_on(static_cast<std::uint8_t>(operand[0]),
                                    style_value.font_weight == FontWeight::bold)
                              ? FontWeight::bold
                              : FontWeight::normal;
      break;
    case sprmCFItalic:
      style.font_style = toggle_on(static_cast<std::uint8_t>(operand[0]),
                                   style_value.font_style == FontStyle::italic)
                             ? FontStyle::italic
                             : FontStyle::normal;
      break;
    case sprmCFStrike:
      style.font_line_through =
          toggle_on(static_cast<std::uint8_t>(operand[0]),
                    style_value.font_line_through.value_or(false));
      break;
    case sprmCHighlight:
      style.background_color = ico_color(static_cast<std::uint8_t>(operand[0]));
      break;
    case sprmCKul:
      style.font_underline = operand[0] != 0;
      break;
    case sprmCIco:
      style.font_color = ico_color(static_cast<std::uint8_t>(operand[0]));
      break;
    case sprmCHps: {
      const auto half_points =
          util::byte::from_little_endian<std::uint16_t>(operand);
      if (half_points < 2 || half_points > 3276) {
        throw std::runtime_error("doc: sprmCHps value out of range");
      }
      style.font_size = Measure(half_points / 2.0, DynamicUnit("pt"));
    } break;
    case sprmCRgFtc0: {
      // SttbfFfn index ([MS-DOC] 2.6.1); with 0 entries it MUST be 0 and the
      // font stays as it is.
      const auto ftc = static_cast<std::int16_t>(
          util::byte::from_little_endian<std::uint16_t>(operand));
      if (ftc < 0 || (font_names.empty() ? ftc != 0
                                         : static_cast<std::size_t>(ftc) >=
                                               font_names.size())) {
        throw std::runtime_error("doc: sprmCRgFtc0 font index out of range");
      }
      if (!font_names.empty()) {
        style.font_name = font_names[static_cast<std::size_t>(ftc)];
      }
    } break;
    case sprmCCv: {
      // COLORREF ([MS-DOC] 2.9.43): red, green, blue, fAuto.
      if (static_cast<std::uint8_t>(operand[3]) == 0xFF) {
        style.font_color = std::nullopt; // cvAuto
      } else {
        style.font_color = Color(static_cast<std::uint8_t>(operand[0]),
                                 static_cast<std::uint8_t>(operand[1]),
                                 static_cast<std::uint8_t>(operand[2]));
      }
    } break;
    default:
      break; // not modelled
    }
  });

  return style;
}

ParagraphStyle text::apply_paragraph_sprms(ParagraphStyle style,
                                           const std::string_view grpprl) {
  for_each_prl(grpprl, [&](const std::uint16_t opcode,
                           const std::string_view operand) {
    switch (opcode) {
    case sprmPJc80:
    case sprmPJc:
      // 3 justifies, as do the distributed and kashida variants after it.
      switch (static_cast<std::uint8_t>(operand[0])) {
      case 0:
        style.text_align = TextAlign::left;
        break;
      case 1:
        style.text_align = TextAlign::center;
        break;
      case 2:
        style.text_align = TextAlign::right;
        break;
      default:
        style.text_align = TextAlign::justify;
        break;
      }
      break;
    case sprmPFPageBreakBefore:
      style.break_before = operand[0] != 0 ? BreakType::page : BreakType::none;
      break;
    case sprmPDxaLeft80:
    case sprmPDxaLeft:
      style.margin.left = twips(operand_i16(operand));
      break;
    case sprmPDxaRight80:
    case sprmPDxaRight:
      style.margin.right = twips(operand_i16(operand));
      break;
    case sprmPDxaLeft180:
    case sprmPDxaLeft1:
      style.text_indent = twips(operand_i16(operand));
      break;
    case sprmPDyaBefore:
      style.margin.top =
          twips(util::byte::from_little_endian<std::uint16_t>(operand));
      break;
    case sprmPDyaAfter:
      style.margin.bottom =
          twips(util::byte::from_little_endian<std::uint16_t>(operand));
      break;
    case sprmPDyaLine: {
      // LSPD ([MS-DOC] 2.9.146): in 240ths of a line when fMultLinespace is
      // set, otherwise in twips, at least that (positive) or exactly that
      // (negative); 0 is single spacing.
      const std::int16_t dya_line = operand_i16(operand);
      if (operand_i16(operand.substr(2)) != 0) {
        style.line_height = Measure(dya_line / 2.4, DynamicUnit("%"));
      } else if (dya_line == 0) {
        style.line_height = std::nullopt;
      } else {
        style.line_height = twips(std::abs(dya_line));
      }
    } break;
    default:
      break; // not modelled
    }
  });

  return style;
}

PageLayout text::apply_section_sprms(PageLayout layout,
                                     const std::string_view grpprl) {
  for_each_prl(
      grpprl, [&](const std::uint16_t opcode, const std::string_view operand) {
        // A negative top or bottom margin is one the header or footer cannot
        // push ([MS-DOC] 2.6.4); the page box takes its size either way.
        switch (opcode) {
        case sprmSBOrientation:
          if (const auto orientation = static_cast<std::uint8_t>(operand[0]);
              orientation == 1 || orientation == 2) {
            layout.print_orientation = orientation == 2
                                           ? PrintOrientation::landscape
                                           : PrintOrientation::portrait;
          }
          break;
        case sprmSXaPage:
          layout.width =
              twips(util::byte::from_little_endian<std::uint16_t>(operand));
          break;
        case sprmSYaPage:
          layout.height =
              twips(util::byte::from_little_endian<std::uint16_t>(operand));
          break;
        case sprmSDxaLeft:
          layout.margin.left =
              twips(util::byte::from_little_endian<std::uint16_t>(operand));
          break;
        case sprmSDxaRight:
          layout.margin.right =
              twips(util::byte::from_little_endian<std::uint16_t>(operand));
          break;
        case sprmSDyaTop:
          layout.margin.top = twips(std::abs(operand_i16(operand)));
          break;
        case sprmSDyaBottom:
          layout.margin.bottom = twips(std::abs(operand_i16(operand)));
          break;
        default:
          break; // not modelled
        }
      });

  return layout;
}

std::vector<std::string> text::read_font_names(std::istream &table_stream,
                                               const FcLcb sttbf_ffn) {
  std::vector<std::string> result;
  if (sttbf_ffn.lcb == 0) {
    return result;
  }
  table_stream.seekg(sttbf_ffn.fc);

  const std::string body =
      util::byte_stream::read_u8s(table_stream, sttbf_ffn.lcb);
  util::byte_string::Reader cursor(body);

  // SttbfFfn is a non-extended STTB ([MS-DOC] 2.2.4, 2.9.286): u16 cData,
  // u16 cbExtra (0), then per entry a u8 byte count and an FFN.
  const auto c_data = cursor.read<std::uint16_t>();
  if (c_data == 0xFFFF) {
    throw std::runtime_error("doc: unexpected extended SttbfFfn");
  }
  const auto cb_extra = cursor.read<std::uint16_t>();

  // Inside the declared length, a quirk of one font name does not refuse the
  // document: an odd byte after the name and any cbExtra bytes are skipped,
  // and a name without its NUL takes all of its units.
  result.reserve(c_data);
  for (std::size_t i = 0; i < c_data; ++i) {
    const auto cch_data = cursor.read<std::uint8_t>();
    if (cch_data < sizeof(FfnFixed)) {
      throw std::runtime_error("doc: FFN too short");
    }
    cursor.skip(sizeof(FfnFixed));
    // xszFfn: NUL-terminated UTF-16; an alternative name may follow it.
    const std::size_t name_bytes = cch_data - sizeof(FfnFixed);
    std::u16string name;
    bool terminated = false;
    for (std::size_t unit = 0; unit < name_bytes / 2; ++unit) {
      const auto character = cursor.read<char16_t>();
      terminated = terminated || character == 0;
      if (!terminated) {
        name.push_back(character);
      }
    }
    cursor.skip(name_bytes % 2 + cb_extra);
    result.push_back(util::string::u16string_to_string(name));
  }

  return result;
}

text::StyleSheet
text::read_style_sheet(std::istream &table_stream, const FcLcb stshf,
                       const std::span<const std::string> font_names) {
  StyleSheet::Entry base{.paragraph = {}, .text = default_character_style()};
  if (stshf.lcb == 0) {
    return {std::move(base), {}};
  }
  table_stream.seekg(stshf.fc);
  const std::string bytes =
      util::byte_stream::read_u8s(table_stream, stshf.lcb);
  util::byte_string::Reader cursor(bytes);

  const auto take = [](util::byte_string::Reader &reader,
                       const std::size_t size) {
    if (reader.remaining() < size) {
      throw std::runtime_error("doc: style sheet overruns its length");
    }
    const std::string_view result = reader.rest().substr(0, size);
    reader.skip(size);
    return result;
  };

  // LPStshi ([MS-DOC] 2.9.136): the Stshif's cstd and cbSTDBaseInFile, and
  // after four more fields the default font, ftcAsci.
  const auto cb_stshi = cursor.read<std::uint16_t>();
  util::byte_string::Reader stshi(take(cursor, cb_stshi));
  const auto cstd = stshi.read<std::uint16_t>();
  const auto cb_std_base = stshi.read<std::uint16_t>();
  if (cb_std_base < 10) {
    throw std::runtime_error("doc: STD base too short");
  }
  if (stshi.remaining() >= 10) {
    stshi.skip(8);
    if (const auto ftc = stshi.read<std::uint16_t>(); ftc < font_names.size()) {
      base.text.font_name = font_names[ftc];
    }
  }

  // Each LPStd ([MS-DOC] 2.9.135) is a u16 size and an STD of that size. The
  // STD opens with the StdfBase: sti, then stk and istdBase, then cupx. The
  // name follows the Stdf, then the paragraph style's UpxPapx (istd and
  // grpprl) and UpxChpx, each u16-sized and padded to even.
  struct Definition {
    std::uint16_t istd_base;
    std::string_view papx;
    std::string_view chpx;
  };
  std::vector<std::optional<Definition>> definitions(cstd);
  for (std::uint16_t istd = 0; istd < cstd; ++istd) {
    const auto cb_std = cursor.read<std::uint16_t>();
    util::byte_string::Reader std_reader(take(cursor, cb_std));
    if (cb_std == 0) {
      continue;
    }
    const std::string_view stdf = take(std_reader, cb_std_base);
    const auto stk_and_base =
        util::byte::from_little_endian<std::uint16_t>(stdf.substr(2));
    const auto cupx = static_cast<std::uint8_t>(stdf[4]) & 0x0F;
    if ((stk_and_base & 0x0F) != 1) {
      continue; // not a paragraph style
    }

    const auto cch = std_reader.read<std::uint16_t>();
    take(std_reader, (static_cast<std::size_t>(cch) + 1) * 2);

    std::array<std::string_view, 2> upx;
    for (std::size_t i = 0; i < std::min<std::size_t>(cupx, upx.size()); ++i) {
      const auto cb_upx = std_reader.read<std::uint16_t>();
      upx[i] = take(std_reader, cb_upx);
      take(std_reader,
           std::min<std::size_t>(cb_upx % 2, std_reader.remaining()));
    }
    if (upx[0].size() == 1) {
      throw std::runtime_error("doc: UpxPapx without its istd");
    }
    definitions[istd] =
        Definition{.istd_base = static_cast<std::uint16_t>(stk_and_base >> 4),
                   .papx = upx[0].empty() ? upx[0] : upx[0].substr(2),
                   .chpx = upx[1]};
  }

  // Each style lies on top of its istdBase, 0x0FFF meaning none.
  std::vector<std::optional<StyleSheet::Entry>> entries(cstd);
  for (std::uint16_t istd = 0; istd < cstd; ++istd) {
    // The unresolved styles from `istd` down its chain; longer than `cstd`
    // only through a cycle.
    std::vector<std::uint16_t> chain;
    for (std::uint16_t at = istd;
         at < cstd && definitions[at].has_value() && !entries[at].has_value();
         at = definitions[at]->istd_base) {
      if (chain.size() == cstd) {
        throw std::runtime_error("doc: style sheet istdBase cycle");
      }
      chain.push_back(at);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
      const Definition &definition = *definitions[*it];
      StyleSheet::Entry entry =
          definition.istd_base < cstd &&
                  entries[definition.istd_base].has_value()
              ? *entries[definition.istd_base]
              : base;
      entry.paragraph = apply_paragraph_sprms(entry.paragraph, definition.papx);
      entry.text =
          apply_character_sprms(entry.text, definition.chpx, font_names);
      entries[*it] = std::move(entry);
    }
  }

  return {std::move(base), std::move(entries)};
}

PageLayout text::read_page_layout(std::istream &document_stream,
                                  std::istream &table_stream,
                                  const FcLcb plcf_sed) {
  const PageLayout layout = default_page_layout();
  if (plcf_sed.lcb == 0) {
    return layout;
  }
  table_stream.seekg(plcf_sed.fc);
  const std::string plc_bytes =
      util::byte_stream::read_u8s(table_stream, plcf_sed.lcb);
  const PlcfSedMap plc(plc_bytes.data(), plc_bytes.size());
  if (plc.n() == 0 || plc.aData(0).fcSepx == 0xFFFFFFFF) {
    return layout;
  }

  // Sepx ([MS-DOC] 2.9.245): a signed size, then the grpprl.
  document_stream.seekg(plc.aData(0).fcSepx);
  const auto cb = util::byte_stream::read<std::int16_t>(document_stream);
  if (cb < 0) {
    throw std::runtime_error("doc: negative Sepx size");
  }
  return apply_section_sprms(
      layout, util::byte_stream::read_u8s(document_stream,
                                          static_cast<std::size_t>(cb)));
}

} // namespace odr::internal::oldms
