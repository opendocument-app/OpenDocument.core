#include <odr/internal/rtf/rtf_parser.hpp>

#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <odr/internal/encoding/transcode.hpp>
#include <odr/internal/rtf/rtf_element_registry.hpp>
#include <odr/internal/rtf/rtf_state.hpp>
#include <odr/internal/rtf/rtf_token.hpp>
#include <odr/internal/rtf/rtf_tokenizer.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>

namespace odr::internal {

namespace {

using namespace rtf;

constexpr char32_t replacement_character = 0xfffd;

/// Destinations the spec defines that carry no body text at this fidelity.
/// `{\*`-marked ones need no entry — the `\*` control symbol discards those
/// whatever their destination is.
const std::unordered_set<std::string_view> &discarded_destinations() {
  static const std::unordered_set<std::string_view> destinations{
      // header tables
      "fonttbl", "filetbl", "colortbl", "stylesheet", "listtable",
      "listoverridetable", "revtbl", "rsidtbl", "info",
      // deferred document parts, see `PLAN.md`
      "header", "headerl", "headerr", "headerf", "footer", "footerl", "footerr",
      "footerf", "footnote", "annotation", "atnauthor", "atndate", "atnid",
      "atnparent", "atnref", "atntime", "atnicn",
      // indexes and bookmarks: markers, not text
      "xe", "tc", "txe", "bkmkstart", "bkmkend",
      // a field instruction is normally `{\*\fldinst}`, but not every writer
      // marks it; `\fldrslt` is left to flow, which is the cached-result-only
      // position `oldms/text` takes too
      "fldinst",
      // pictures and embedded objects, see `PLAN.md` stage 5. `\nonshppict`
      // is the un-marked twin of `{\*\shppict}` and would otherwise emit
      // every image a second time
      "pict", "nonshppict", "object", "objdata", "result", "do", "shptxt",
      // font and style descriptors
      "falt", "fname", "ffname", "panose", "keycode",
      // math and drawing
      "mmath", "factoidname", "datafield", "template", "private"};
  return destinations;
}

/// Control words that stand for exactly one character.
const std::unordered_map<std::string_view, char32_t> &literal_characters() {
  static const std::unordered_map<std::string_view, char32_t> characters{
      {"emdash", 0x2014},    {"endash", 0x2013},  {"emspace", 0x2003},
      {"enspace", 0x2002},   {"qmspace", 0x2005}, {"bullet", 0x2022},
      {"lquote", 0x2018},    {"rquote", 0x2019},  {"ldblquote", 0x201c},
      {"rdblquote", 0x201d}, {"ltrmark", 0x200e}, {"rtlmark", 0x200f},
      {"zwj", 0x200d},       {"zwnj", 0x200c},    {"zwbo", 0x200b},
      {"zwnbo", 0xfeff}};
  return characters;
}

/// The `\ansicpgN` code page as a @ref TextEncoding, or windows-1252 for one
/// this build has no row for.
TextEncoding encoding_by_codepage(const std::int32_t codepage) {
  switch (codepage) {
  case 866:
    return TextEncoding::ibm866;
  case 874:
    return TextEncoding::windows_874;
  case 932:
    return TextEncoding::shift_jis;
  case 936:
    return TextEncoding::gb18030;
  case 949:
    return TextEncoding::euc_kr;
  case 950:
    return TextEncoding::big5;
  case 1250:
    return TextEncoding::windows_1250;
  case 1251:
    return TextEncoding::windows_1251;
  case 1252:
    return TextEncoding::windows_1252;
  case 1253:
    return TextEncoding::windows_1253;
  case 1254:
    return TextEncoding::windows_1254;
  case 1255:
    return TextEncoding::windows_1255;
  case 1256:
    return TextEncoding::windows_1256;
  case 1257:
    return TextEncoding::windows_1257;
  case 1258:
    return TextEncoding::windows_1258;
  case 10000:
    return TextEncoding::macintosh;
  case 10007:
    return TextEncoding::x_mac_cyrillic;
  case 20866:
    return TextEncoding::koi8_r;
  case 21866:
    return TextEncoding::koi8_u;
  case 28591:
    return TextEncoding::iso_8859_1;
  case 28592:
    return TextEncoding::iso_8859_2;
  case 28593:
    return TextEncoding::iso_8859_3;
  case 28594:
    return TextEncoding::iso_8859_4;
  case 28595:
    return TextEncoding::iso_8859_5;
  case 28596:
    return TextEncoding::iso_8859_6;
  case 28597:
    return TextEncoding::iso_8859_7;
  case 28598:
    return TextEncoding::iso_8859_8;
  case 28600:
    return TextEncoding::iso_8859_10;
  case 28603:
    return TextEncoding::iso_8859_13;
  case 28604:
    return TextEncoding::iso_8859_14;
  case 28605:
    return TextEncoding::iso_8859_15;
  case 28606:
    return TextEncoding::iso_8859_16;
  case 50220:
  case 50221:
  case 50222:
    return TextEncoding::iso_2022_jp;
  case 50225:
    return TextEncoding::iso_2022_kr;
  case 51932:
    return TextEncoding::euc_jp;
  case 65001:
    return TextEncoding::utf8;
  default:
    return TextEncoding::windows_1252;
  }
}

bool is_high_surrogate(const char16_t unit) {
  return unit >= 0xd800 && unit <= 0xdbff;
}

bool is_low_surrogate(const char16_t unit) {
  return unit >= 0xdc00 && unit <= 0xdfff;
}

class TreeBuilder final {
public:
  TreeBuilder(ElementRegistry &registry, std::istream &in)
      : m_registry{&registry}, m_tokenizer{in} {}

  ElementIdentifier parse();

private:
  ElementRegistry *m_registry{nullptr};
  Tokenizer m_tokenizer;
  State m_state;

  ElementIdentifier m_root{null_element_id};
  ElementIdentifier m_paragraph{null_element_id};

  /// The run so far, still in @ref m_encoding — `\'hh` yields a *byte*, so two
  /// escapes can be one character and decoding per escape would corrupt every
  /// multibyte run.
  std::string m_bytes;
  /// The current paragraph's text so far, already utf-8.
  std::string m_text;
  TextEncoding m_encoding{TextEncoding::windows_1252};

  /// A `\uN` high surrogate waiting for the low one that completes it.
  char16_t m_high_surrogate{0};
  /// `\ucN` fallback characters still to be skipped.
  std::int32_t m_skip{0};

  /// Whether this token is one of the `\ucN` fallback characters, which counts
  /// it as consumed. Any control word or symbol counts as one character, as
  /// does a `\binN` together with its payload.
  bool consume_skip();

  void handle_control_word(const ControlWord &word);
  void handle_control_symbol(const ControlSymbol &symbol);
  void handle_text(std::string_view bytes);

  void unicode_character(std::int32_t value);
  void append_character(char32_t character);
  void append_text(std::string_view text);

  void resolve_surrogate();
  void flush_bytes();
  /// End the run: an unpaired surrogate becomes U+FFFD, the bytes are decoded.
  void flush_run();

  void ensure_paragraph();
  void flush_text();
  void end_paragraph();
  void line_break();
  void page_break();
  void finish();
};

ElementIdentifier TreeBuilder::parse() {
  auto [root_id, _] = m_registry->create_element(ElementType::root);
  m_root = root_id;

  while (true) {
    const Token token = m_tokenizer.read_token();

    if (std::holds_alternative<End>(token)) {
      if (m_state.depth() > 1) {
        throw std::runtime_error("rtf: group left open at end of file");
      }
      break;
    }
    if (std::holds_alternative<GroupOpen>(token)) {
      flush_run();
      m_skip = 0;
      m_state.save();
      continue;
    }
    if (std::holds_alternative<GroupClose>(token)) {
      flush_run();
      m_skip = 0;
      m_state.restore();
      continue;
    }
    if (const auto *word = std::get_if<ControlWord>(&token)) {
      if (!consume_skip()) {
        handle_control_word(*word);
      }
      continue;
    }
    if (const auto *symbol = std::get_if<ControlSymbol>(&token)) {
      if (!consume_skip()) {
        handle_control_symbol(*symbol);
      }
      continue;
    }
    if (const auto *hex = std::get_if<HexEscape>(&token)) {
      if (!consume_skip() && !m_state.current().discard) {
        m_bytes.push_back(hex->byte);
      }
      continue;
    }
    if (std::holds_alternative<Binary>(token)) {
      // no destination reads the payload yet; it counts as one skippable
      // character together with its `\binN`
      consume_skip();
      continue;
    }
    if (const auto *text = std::get_if<Text>(&token)) {
      handle_text(text->bytes);
      continue;
    }
  }

  finish();
  return m_root;
}

bool TreeBuilder::consume_skip() {
  if (m_skip <= 0) {
    return false;
  }
  --m_skip;
  return true;
}

void TreeBuilder::handle_control_word(const ControlWord &word) {
  const std::string &name = word.name;

  if (discarded_destinations().contains(name)) {
    m_state.current().discard = true;
    return;
  }

  // The document encoding is set in the header, which is never discarded, but
  // it is read before the discard check so a header written inside a group we
  // drop still lands.
  if (name == "ansi") {
    m_encoding = TextEncoding::windows_1252;
    return;
  }
  if (name == "mac") {
    m_encoding = TextEncoding::macintosh;
    return;
  }
  if (name == "pc" || name == "pca") {
    // IBM code pages 437 and 850, neither of which `internal/encoding` has
    m_encoding = TextEncoding::windows_1252;
    return;
  }
  if (name == "ansicpg") {
    m_encoding = encoding_by_codepage(word.parameter.value_or(1252));
    return;
  }

  if (name == "uc") {
    m_state.current().uc = std::max(0, word.parameter.value_or(1));
    return;
  }
  if (name == "u") {
    unicode_character(word.parameter.value_or(0));
    return;
  }

  if (m_state.current().discard) {
    return;
  }

  if (name == "par" || name == "sect") {
    end_paragraph();
    return;
  }
  if (name == "page") {
    page_break();
    return;
  }
  if (name == "line" || name == "softline") {
    line_break();
    return;
  }
  if (name == "tab") {
    append_text("\t");
    return;
  }
  // Without the table reconstruction of `PLAN.md` stage 4, a cell boundary
  // still reads as a column break and a row as a paragraph.
  if (name == "cell" || name == "nestcell") {
    append_text("\t");
    return;
  }
  if (name == "row" || name == "nestrow") {
    end_paragraph();
    return;
  }

  if (const auto it = literal_characters().find(name);
      it != literal_characters().end()) {
    append_character(it->second);
    return;
  }

  // an unknown control word is ignored (*Conventions of an RTF Reader*)
}

void TreeBuilder::handle_control_symbol(const ControlSymbol &symbol) {
  switch (symbol.symbol) {
  case '*':
    // an ignorable destination: everything to the matching `}` is dropped
    m_state.current().discard = true;
    break;
  case '\\':
  case '{':
  case '}':
    if (!m_state.current().discard) {
      m_bytes.push_back(symbol.symbol);
    }
    break;
  case '~':
    append_character(0x00a0); // non-breaking space
    break;
  case '_':
    append_character(0x2011); // non-breaking hyphen
    break;
  case '\r':
  case '\n':
    // `\<CR>` and `\<LF>` are `\par` (*Conventions of an RTF Reader*)
    if (!m_state.current().discard) {
      end_paragraph();
    }
    break;
  case '-': // optional hyphen: nothing is rendered where it does not break
  case ':': // subentry of an index entry
  case '|': // formula character
  default:
    break;
  }
}

void TreeBuilder::handle_text(std::string_view bytes) {
  if (m_skip > 0) {
    const auto skipped =
        std::min<std::size_t>(static_cast<std::size_t>(m_skip), bytes.size());
    m_skip -= static_cast<std::int32_t>(skipped);
    bytes.remove_prefix(skipped);
  }
  if (bytes.empty() || m_state.current().discard) {
    return;
  }
  m_bytes.append(bytes);
}

void TreeBuilder::unicode_character(const std::int32_t value) {
  m_skip = m_state.current().uc;

  if (m_state.current().discard) {
    return;
  }

  // Content between the two halves breaks the pair, and the bytes have to
  // come out after the replacement character that ends the run.
  if (!m_bytes.empty()) {
    flush_run();
  }

  // the parameter is a signed 16-bit code *unit*, so U+F020 arrives as -4064
  std::int32_t value_folded = value;
  if (value_folded < 0) {
    value_folded += 0x10000;
  }
  if (value_folded < 0 || value_folded > 0xffff) {
    resolve_surrogate();
    util::string::append_c32(replacement_character, m_text);
    return;
  }

  const auto unit = static_cast<char16_t>(value_folded);

  if (m_high_surrogate != 0) {
    if (is_low_surrogate(unit)) {
      const auto character = static_cast<char32_t>(
          0x10000 + ((m_high_surrogate - 0xd800) << 10) + (unit - 0xdc00));
      m_high_surrogate = 0;
      util::string::append_c32(character, m_text);
      return;
    }
    resolve_surrogate();
  }

  if (is_high_surrogate(unit)) {
    m_high_surrogate = unit;
    return;
  }
  if (is_low_surrogate(unit)) {
    util::string::append_c32(replacement_character, m_text);
    return;
  }
  util::string::append_c32(unit, m_text);
}

void TreeBuilder::append_character(const char32_t character) {
  if (m_state.current().discard) {
    return;
  }
  flush_run();
  util::string::append_c32(character, m_text);
}

void TreeBuilder::append_text(const std::string_view text) {
  if (m_state.current().discard) {
    return;
  }
  flush_run();
  m_text.append(text);
}

void TreeBuilder::resolve_surrogate() {
  if (m_high_surrogate == 0) {
    return;
  }
  m_high_surrogate = 0;
  util::string::append_c32(replacement_character, m_text);
}

void TreeBuilder::flush_bytes() {
  if (m_bytes.empty()) {
    return;
  }
  if (text_encoding_is_decodable(m_encoding)) {
    m_text += encoding::to_utf8(m_bytes, m_encoding);
  } else {
    // A named-but-not-decoded multibyte encoding. The ascii skeleton is the
    // same in all of them, and a writer emitting cjk generally emits `\uN`
    // alongside, which is decoded whatever the run's encoding.
    for (const char byte : m_bytes) {
      if (static_cast<unsigned char>(byte) < 0x80) {
        m_text.push_back(byte);
      } else {
        util::string::append_c32(replacement_character, m_text);
      }
    }
  }
  m_bytes.clear();
}

void TreeBuilder::flush_run() {
  resolve_surrogate();
  flush_bytes();
}

void TreeBuilder::ensure_paragraph() {
  if (m_paragraph != null_element_id) {
    return;
  }
  auto [id, _] = m_registry->create_element(ElementType::paragraph);
  m_registry->append_child(m_root, id);
  m_paragraph = id;
}

void TreeBuilder::flush_text() {
  if (m_text.empty()) {
    return;
  }
  auto [id, element, entry] = m_registry->create_text_element();
  entry.text = std::move(m_text);
  m_text.clear();
  m_registry->append_child(m_paragraph, id);
}

void TreeBuilder::end_paragraph() {
  flush_run();
  ensure_paragraph();
  flush_text();
  m_paragraph = null_element_id;
}

void TreeBuilder::line_break() {
  flush_run();
  ensure_paragraph();
  flush_text();
  auto [id, _] = m_registry->create_element(ElementType::line_break);
  m_registry->append_child(m_paragraph, id);
}

void TreeBuilder::page_break() {
  end_paragraph();
  auto [id, _] = m_registry->create_element(ElementType::page_break);
  m_registry->append_child(m_root, id);
}

void TreeBuilder::finish() {
  flush_run();
  // paragraphs open lazily, so the trailing `\par` adds no empty one
  if (m_paragraph == null_element_id && m_text.empty()) {
    return;
  }
  ensure_paragraph();
  flush_text();
}

} // namespace

ElementIdentifier rtf::parse_tree(ElementRegistry &registry, std::istream &in) {
  return TreeBuilder(registry, in).parse();
}

} // namespace odr::internal
