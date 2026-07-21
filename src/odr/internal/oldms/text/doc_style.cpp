#include <odr/internal/oldms/text/doc_style.hpp>

#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <istream>
#include <stdexcept>

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
  return Color(ico_colors[ico]);
}

/// ToggleOperand ([MS-DOC] 2.9.327) against the (unmodelled) style value:
/// 0x80 matches the style, 0x81 inverts it; styles default to off.
bool toggle_on(const std::uint8_t value) {
  switch (value) {
  case 0x00:
  case 0x80:
    return false;
  case 0x01:
  case 0x81:
    return true;
  default:
    throw std::runtime_error("doc: unexpected ToggleOperand value");
  }
}

std::uint16_t read_u16(const std::string_view bytes, const std::size_t at) {
  std::uint16_t value;
  std::memcpy(&value, bytes.data() + at, sizeof(value));
  return value;
}

} // namespace

} // namespace odr::internal::oldms::text

namespace odr::internal::oldms {

const char *text::StyleRegistry::intern_font_name(const std::string &name) {
  if (const auto it = std::ranges::find(m_font_names, name);
      it != m_font_names.end()) {
    return it->c_str();
  }
  return m_font_names.emplace_back(name).c_str();
}

std::uint32_t text::StyleRegistry::add_style(TextStyle style) {
  m_styles.push_back(std::move(style));
  return static_cast<std::uint32_t>(m_styles.size() - 1);
}

const TextStyle &
text::StyleRegistry::text_style(const std::uint32_t index) const {
  return m_styles.at(index);
}

TextStyle
text::apply_character_sprms(TextStyle style, const std::string_view grpprl,
                            const std::vector<const char *> &font_names) {
  std::size_t at = 0;
  while (at + sizeof(Sprm) <= grpprl.size()) {
    Sprm sprm;
    std::memcpy(&sprm, grpprl.data() + at, sizeof(sprm));
    const std::uint16_t opcode = std::bit_cast<std::uint16_t>(sprm);
    at += sizeof(sprm);

    std::size_t operand_size;
    if (const int fixed_size = sprm.operand_size(); fixed_size >= 0) {
      operand_size = static_cast<std::size_t>(fixed_size);
    } else {
      // spra == 6: the first operand byte is the size of the remainder. The
      // two SPRMs encoded differently (sprmTDefTable, sprmPChgTabs with
      // cb == 0xFF) are not character properties and must not appear here.
      if (at >= grpprl.size()) {
        throw std::runtime_error("doc: Prl operand overruns grpprl");
      }
      operand_size = static_cast<std::uint8_t>(grpprl[at]);
      ++at;
      if (opcode == 0xD608 || (opcode == 0xC615 && operand_size == 0xFF)) {
        throw std::runtime_error("doc: unexpected table SPRM in a Chpx");
      }
    }
    if (at + operand_size > grpprl.size()) {
      throw std::runtime_error("doc: Prl operand overruns grpprl");
    }
    const std::string_view operand = grpprl.substr(at, operand_size);
    at += operand_size;

    switch (opcode) {
    case sprmCFBold:
      style.font_weight = toggle_on(static_cast<std::uint8_t>(operand[0]))
                              ? FontWeight::bold
                              : FontWeight::normal;
      break;
    case sprmCFItalic:
      style.font_style = toggle_on(static_cast<std::uint8_t>(operand[0]))
                             ? FontStyle::italic
                             : FontStyle::normal;
      break;
    case sprmCFStrike:
      style.font_line_through =
          toggle_on(static_cast<std::uint8_t>(operand[0]));
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
      const std::uint16_t half_points = read_u16(operand, 0);
      if (half_points < 2 || half_points > 3276) {
        throw std::runtime_error("doc: sprmCHps value out of range");
      }
      style.font_size = Measure(half_points / 2.0, DynamicUnit("pt"));
    } break;
    case sprmCRgFtc0: {
      // SttbfFfn index ([MS-DOC] 2.6.1); with 0 entries the value MUST be 0
      // and the (unmodelled) style-sheet default font applies — leave unset.
      const auto ftc = static_cast<std::int16_t>(read_u16(operand, 0));
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
  }
  if (at != grpprl.size()) {
    throw std::runtime_error("doc: grpprl does not hold whole Prls");
  }

  return style;
}

std::vector<std::string> text::read_font_names(std::istream &table_stream,
                                               const FcLcb sttbf_ffn) {
  std::vector<std::string> result;
  if (sttbf_ffn.lcb == 0) {
    return result;
  }
  table_stream.seekg(sttbf_ffn.fc);

  // SttbfFfn is a non-extended STTB ([MS-DOC] 2.2.4, 2.9.286): u16 cData,
  // u16 cbExtra (0), then per entry a u8 byte count and an FFN.
  const auto c_data = util::byte_stream::read<std::uint16_t>(table_stream);
  if (c_data == 0xFFFF) {
    throw std::runtime_error("doc: unexpected extended SttbfFfn");
  }
  const auto cb_extra = util::byte_stream::read<std::uint16_t>(table_stream);

  result.reserve(c_data);
  for (std::uint16_t i = 0; i < c_data; ++i) {
    const auto cch_data = util::byte_stream::read<std::uint8_t>(table_stream);
    if (cch_data < sizeof(FfnFixed)) {
      throw std::runtime_error("doc: FFN too short");
    }
    const auto ffn = util::byte_stream::read<FfnFixed>(table_stream);
    (void)ffn;

    // xszFfn: null-terminated UTF-16 within the remaining bytes; an
    // alternative name (xszAlt) may follow the terminator.
    const std::size_t name_units = (cch_data - sizeof(FfnFixed)) / 2;
    std::u16string name = read_string_uncompressed(table_stream, name_units);
    if ((cch_data - sizeof(FfnFixed)) % 2 != 0) {
      table_stream.ignore(1);
    }
    if (const std::size_t nul = name.find(u'\0'); nul != std::u16string::npos) {
      name.resize(nul);
    }
    result.push_back(util::string::u16string_to_string(name));

    table_stream.ignore(cb_extra);
  }

  return result;
}

} // namespace odr::internal::oldms
