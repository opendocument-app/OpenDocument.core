#include <odr/internal/iwork/iwork_text.hpp>

#include <odr/internal/iwork/iwork_budget.hpp>
#include <odr/internal/iwork/iwork_element_registry.hpp>
#include <odr/internal/iwork/iwork_protobuf.hpp>
#include <odr/internal/iwork/iwork_types.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odr::internal::iwork {

namespace {

/// `U+2028 LINE SEPARATOR` — a line break inside a paragraph.
constexpr std::string_view line_separator = "\xe2\x80\xa8";
/// `U+FFFC OBJECT REPLACEMENT CHARACTER` — where a drawable is anchored in the
/// text. Nothing reads drawables anchored in a text flow yet, so the anchor is
/// dropped rather than rendered as a glyph.
constexpr std::string_view object_replacement = "\xef\xbf\xbc";

/// The paragraph mark ends the paragraph it belongs to. Pages writes `\n` and
/// Keynote `\r` — the run table is what says where a paragraph starts either
/// way, so this only decides whether the mark is part of the text.
bool is_paragraph_mark(const char c) { return c == '\n' || c == '\r'; }

/// The character index each paragraph of @p storage starts at. Paragraph
/// boundaries are the run table's rather than every mark in the text — the two
/// agree today, but the table is what says so.
std::vector<std::uint64_t> paragraph_starts(const Message &storage) {
  std::vector<std::uint64_t> result;

  if (const std::optional<std::string_view> table =
          storage.bytes_field(text_storage::paragraph_styles);
      table.has_value()) {
    for (const Field &entry :
         Message(*table).repeated_field(attribute_table::entries)) {
      if (entry.type != WireType::length_delimited) {
        throw std::runtime_error("iwork: malformed paragraph style table");
      }
      const Message run(entry.bytes);
      result.push_back(
          run.number_field(attribute_table_entry::character_index).value_or(0));
    }
  }

  // no table, or an empty one: either way the body starts at its first
  // character
  if (result.empty() || result.front() != 0) {
    result.insert(result.begin(), 0);
  }
  return result;
}

/// Fills @p paragraph_id with the text of one paragraph, breaking it at the
/// line separators it holds.
void parse_paragraph(ElementRegistry &registry, Budget &budget,
                     const ElementIdentifier paragraph_id,
                     std::string_view content) {
  const auto append_text = [&](const std::string_view part) {
    std::string text(part);
    util::string::replace_all(text, std::string(object_replacement), "");
    if (text.empty()) {
      return;
    }
    budget.spend_element();
    budget.spend_text(text.size());
    auto [text_id, element, payload] = registry.create_text_element();
    payload.text = std::move(text);
    registry.append_child(paragraph_id, text_id);
  };

  for (std::size_t position = content.find(line_separator);
       position != std::string_view::npos;
       position = content.find(line_separator)) {
    append_text(content.substr(0, position));

    budget.spend_element();
    auto [break_id, element] = registry.create_element(ElementType::line_break);
    registry.append_child(paragraph_id, break_id);

    content.remove_prefix(position + line_separator.size());
  }
  append_text(content);
}

} // namespace

} // namespace odr::internal::iwork

namespace odr::internal {

void iwork::parse_storage(ElementRegistry &registry, Budget &budget,
                          const ElementIdentifier parent_id,
                          const Message &storage) {
  // the text arrives as a small number of large strings; the run tables index
  // it as one
  std::string text;
  for (const Field &part : storage.repeated_field(text_storage::text)) {
    if (part.type != WireType::length_delimited) {
      throw std::runtime_error("iwork: malformed text storage");
    }
    text += part.bytes;
  }

  const std::vector<std::size_t> starts =
      util::string::utf16_offsets(text, paragraph_starts(storage));

  const std::string_view body_text(text);
  for (std::size_t i = 0; i < starts.size(); ++i) {
    const std::size_t begin = starts[i];
    const std::size_t end = i + 1 < starts.size() ? starts[i + 1] : text.size();

    std::string_view content = body_text.substr(begin, end - begin);
    // the paragraph mark belongs to the paragraph it ends, and the last
    // paragraph of a body does not carry one
    if (!content.empty() && is_paragraph_mark(content.back())) {
      content.remove_suffix(1);
    }

    budget.spend_element();
    auto [paragraph_id, paragraph] =
        registry.create_element(ElementType::paragraph);
    registry.append_child(parent_id, paragraph_id);
    parse_paragraph(registry, budget, paragraph_id, content);
  }
}

} // namespace odr::internal
