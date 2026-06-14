#include <odr/internal/oldms/text/doc_parser.hpp>

#include <odr/internal/common/path.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/oldms/text/doc_element_registry.hpp>
#include <odr/internal/oldms/text/doc_helper.hpp>
#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/oldms/text/doc_structs.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <string>
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
std::string clean_text(const std::string &in) {
  std::string out;
  out.reserve(in.size());

  // Open fields; entry is true while still in that field's instruction part.
  // instruction_depth counts those; text is hidden whenever it is > 0.
  std::vector<bool> field_in_instruction;
  int instruction_depth = 0;

  for (const char c : in) {
    switch (c) {
    case '\x13': // field begin: a new field opens in its instruction part
      field_in_instruction.push_back(true);
      ++instruction_depth;
      continue;
    case '\x14': // field separator: the innermost field's instruction ends
      if (!field_in_instruction.empty() && field_in_instruction.back()) {
        field_in_instruction.back() = false;
        --instruction_depth;
      }
      continue;
    case '\x15': // field end: close the innermost field
      if (!field_in_instruction.empty()) {
        if (field_in_instruction.back()) {
          // separator-less field: its instruction part ends here, not earlier
          --instruction_depth;
        }
        field_in_instruction.pop_back();
      }
      continue;
    default:
      break;
    }

    if (instruction_depth > 0) {
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
  table_stream->ignore(fib.fibRgFcLcb->clx.fc);

  const CharacterIndex character_index = read_character_index(*table_stream);

  // Concatenate the decoded pieces of the main document body only (the first
  // ccpText CPs). Pieces are in ascending CP order, so clamp each piece to the
  // remaining budget and stop once it is exhausted.
  const std::size_t ccp_text = fib.ccpText();
  std::size_t consumed_cp = 0;
  std::string body_text;
  for (const auto &entry : character_index) {
    if (consumed_cp >= ccp_text) {
      break;
    }
    const std::size_t take = std::min(entry.length_cp, ccp_text - consumed_cp);
    document_stream->seekg(entry.data_offset);
    body_text += read_string(*document_stream, take, entry.is_compressed);
    consumed_cp += take;
  }

  // Split into paragraphs. The main body always ends with a guard paragraph
  // mark, so drop the resulting trailing empty paragraph.
  auto paragraphs =
      util::string::split(body_text, std::string(1, paragraph_mark));
  if (!paragraphs.empty() && paragraphs.back().empty()) {
    paragraphs.pop_back();
  }

  for (const auto &paragraph : paragraphs) {
    // A paragraph may contain end-of-section / manual page-break characters
    // (0x0C): split on them and mark each boundary with a page_break element.
    const auto pages =
        util::string::split(paragraph, std::string(1, page_break_mark));
    for (std::size_t page_i = 0; page_i < pages.size(); ++page_i) {
      if (page_i > 0) {
        auto [page_break_id, _] =
            registry.create_element(ElementType::page_break);
        registry.append_child(root_id, page_break_id);
      }

      auto [paragraph_id, _] = registry.create_element(ElementType::paragraph);
      registry.append_child(root_id, paragraph_id);

      const auto lines =
          util::string::split(pages[page_i], std::string(1, line_break_mark));
      for (std::uint32_t line_i = 0; line_i < lines.size(); ++line_i) {
        if (line_i > 0) {
          auto [line_id, _] = registry.create_element(ElementType::line_break);
          registry.append_child(paragraph_id, line_id);
        }

        auto [text_id, _, text_element] = registry.create_text_element();
        text_element.text = clean_text(lines[line_i]);
        registry.append_child(paragraph_id, text_id);
      }
    }
  }

  return root_id;
}

} // namespace odr::internal::oldms
