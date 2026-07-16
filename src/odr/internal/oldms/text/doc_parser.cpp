#include <odr/internal/oldms/text/doc_parser.hpp>

#include <odr/internal/common/path.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/oldms/text/doc_element_registry.hpp>
#include <odr/internal/oldms/text/doc_helper.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/oldms/text/doc_structs.hpp>

#include <algorithm>
#include <cstdint>
#include <istream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::oldms {

namespace {

// Paragraph mark (end of paragraph).
constexpr char paragraph_mark = '\r'; // 0x0D
// Manual line break (Shift+Enter); a vertical tab in the .doc text stream.
constexpr char line_break_mark = '\x0B';
// End-of-section / manual page break ([MS-DOC] 2.8.26); surfaced as page_break.
constexpr char page_break_mark = '\x0C';

// Drops anchor/control characters and resolves field codes, keeping only the
// visible text. A field is 0x13 begin ... [0x14 separator] ... 0x15 end
// ([MS-DOC] 2.8.25): the instruction (begin..separator) is hidden, the result
// (separator..end) shown. A separator-less field is hidden entirely.
// Stateful: a field can span style runs and paragraphs, so one cleaner
// processes the whole body in order.
class TextCleaner {
public:
  std::string clean(const std::string_view in) {
    std::string out;
    out.reserve(in.size());

    for (const char c : in) {
      switch (c) {
      case '\x13': // field begin: a new field opens in its instruction part
        m_field_in_instruction.push_back(true);
        ++m_instruction_depth;
        continue;
      case '\x14': // field separator: the innermost field's instruction ends
        if (!m_field_in_instruction.empty() && m_field_in_instruction.back()) {
          m_field_in_instruction.back() = false;
          --m_instruction_depth;
        }
        continue;
      case '\x15': // field end: close the innermost field
        if (!m_field_in_instruction.empty()) {
          if (m_field_in_instruction.back()) {
            // separator-less field: its instruction part ends here, not
            // earlier
            --m_instruction_depth;
          }
          m_field_in_instruction.pop_back();
        }
        continue;
      default:
        break;
      }

      if (m_instruction_depth > 0) {
        continue; // drop the hidden field instruction
      }

      switch (c) {
      case '\x09': // tab: keep as real text
        out.push_back(c);
        break;
      case '\x1E': // non-breaking hyphen
        out.push_back('-');
        break;
      case '\x1F': // optional hyphen: drop
        break;
      default:
        // Drop remaining control/anchor characters (< 0x20); keep the rest.
        if (static_cast<std::uint8_t>(c) >= 0x20) {
          out.push_back(c);
        }
        break;
      }
    }

    return out;
  }

private:
  // Open fields; entry is true while still in that field's instruction part.
  // m_instruction_depth counts those; text is hidden whenever it is > 0.
  std::vector<bool> m_field_in_instruction;
  int m_instruction_depth{0};
};

/// A maximal piece of body text with uniform character formatting.
struct StyledRun {
  std::string text;
  std::uint32_t style_index{0};
};

/// Decodes the main-body pieces (the first `ccp_text` CPs), split further at
/// every character-formatting boundary.
std::vector<StyledRun> decode_styled_runs(
    std::istream &document_stream, const text::CharacterIndex &character_index,
    const text::CharacterStyles &styles, const std::size_t ccp_text) {
  std::vector<StyledRun> runs;
  std::size_t consumed_cp = 0;

  for (const auto &entry : character_index) {
    if (consumed_cp >= ccp_text) {
      break;
    }
    const std::size_t take = std::min(entry.length_cp, ccp_text - consumed_cp);
    const std::size_t bytes_per_cp = entry.is_compressed ? 1 : 2;

    std::size_t cp = 0;
    while (cp < take) {
      const auto fc =
          static_cast<std::uint32_t>(entry.data_offset + cp * bytes_per_cp);
      const std::uint32_t style_index = styles.index_at(fc);

      // CPs of this piece still in the same style run; a boundary that falls
      // inside a 2-byte CP is pushed past it.
      const std::uint32_t chunk_end_fc = styles.chunk_end(fc);
      std::size_t chunk_cp = take - cp;
      if (chunk_end_fc != std::numeric_limits<std::uint32_t>::max()) {
        chunk_cp = std::min<std::size_t>(
            chunk_cp,
            std::max<std::size_t>(1, (chunk_end_fc - fc + bytes_per_cp - 1) /
                                         bytes_per_cp));
      }

      document_stream.seekg(fc);
      std::string chunk_text =
          text::read_string(document_stream, chunk_cp, entry.is_compressed);
      if (!runs.empty() && runs.back().style_index == style_index) {
        runs.back().text += chunk_text;
      } else {
        runs.push_back({std::move(chunk_text), style_index});
      }
      cp += chunk_cp;
    }

    consumed_cp += take;
  }

  return runs;
}

} // namespace

ElementIdentifier text::parse_tree(ElementRegistry &registry,
                                   const abstract::ReadableFilesystem &files) {
  auto [root_id, _] = registry.create_element(ElementType::root);

  const auto document_stream = files.open(AbsPath("/WordDocument"))->stream();
  ParsedFib fib;
  read(*document_stream, fib);

  const std::string tableStreamPath =
      fib.base.fWhichTblStm == 1 ? "/1Table" : "/0Table";
  const auto table_stream = files.open(AbsPath(tableStreamPath))->stream();

  // Font table, interned so TextStyle::font_name pointers stay valid.
  std::vector<const char *> font_names;
  for (const std::string &name :
       read_font_names(*table_stream, fib.fibRgFcLcb->sttbfFfn)) {
    font_names.push_back(registry.intern_font_name(name));
  }

  // Direct character formatting ([MS-DOC] 2.4.6.2); Pcd.Prm modifications are
  // not modelled. Without sprmCHps the size defaults to 20 half-points.
  TextStyle default_style;
  default_style.font_size = Measure(10.0, DynamicUnit("pt"));
  const CharacterStyles styles = read_character_styles(
      *document_stream, *table_stream, fib.fibRgFcLcb->plcfBteChpx,
      default_style, font_names);

  table_stream->seekg(fib.fibRgFcLcb->clx.fc);
  const CharacterIndex character_index = read_character_index(*table_stream);

  const auto ccp_text = static_cast<std::size_t>(fib.ccpText());
  const std::vector<StyledRun> runs =
      decode_styled_runs(*document_stream, character_index, styles, ccp_text);

  // Build the tree: paragraphs are opened lazily so the trailing guard
  // paragraph mark does not produce an extra empty paragraph. Each paragraph
  // and span stores its style; empty paragraphs keep their height through the
  // paragraph style.
  TextCleaner cleaner;
  ElementIdentifier paragraph_id = null_element_id;

  const auto ensure_paragraph = [&](const std::uint32_t style_index) {
    if (paragraph_id == null_element_id) {
      auto [id, paragraph] = registry.create_element(ElementType::paragraph);
      registry.append_child(root_id, id);
      registry.set_element_style(id, styles.style(style_index));
      paragraph_id = id;
    }
  };

  for (const StyledRun &run : runs) {
    std::size_t at = 0;
    while (at <= run.text.size()) {
      const std::size_t control = run.text.find_first_of("\r\x0B\x0C", at);
      const std::size_t segment_end =
          control == std::string::npos ? run.text.size() : control;

      if (std::string cleaned = cleaner.clean(
              std::string_view(run.text).substr(at, segment_end - at));
          !cleaned.empty()) {
        ensure_paragraph(run.style_index);
        auto [span_id, span] = registry.create_element(ElementType::span);
        registry.set_element_style(span_id, styles.style(run.style_index));
        registry.append_child(paragraph_id, span_id);

        auto [text_id, text_element, text_entry] =
            registry.create_text_element();
        text_entry.text = std::move(cleaned);
        registry.append_child(span_id, text_id);
      }

      if (control == std::string::npos) {
        break;
      }
      switch (run.text[control]) {
      case paragraph_mark:
        ensure_paragraph(run.style_index);
        paragraph_id = null_element_id;
        break;
      case page_break_mark: {
        ensure_paragraph(run.style_index);
        paragraph_id = null_element_id;
        auto [page_break_id, page_break] =
            registry.create_element(ElementType::page_break);
        registry.append_child(root_id, page_break_id);
      } break;
      case line_break_mark: {
        ensure_paragraph(run.style_index);
        auto [line_id, line] = registry.create_element(ElementType::line_break);
        registry.append_child(paragraph_id, line_id);
      } break;
      default:
        break;
      }
      at = control + 1;
    }
  }

  return root_id;
}

} // namespace odr::internal::oldms
