#include <odr/internal/iwork/iwork_text.hpp>

#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_budget.hpp>
#include <odr/internal/iwork/iwork_element_registry.hpp>
#include <odr/internal/iwork/iwork_protobuf.hpp>
#include <odr/internal/iwork/iwork_table.hpp>
#include <odr/internal/iwork/iwork_types.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
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
/// text. The anchor itself is dropped; what it stands for is appended after
/// the paragraph that holds it.
constexpr std::string_view object_replacement = "\xef\xbf\xbc";

void parse_table(const Context &context, ElementIdentifier parent_id,
                 std::uint64_t identifier);

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
void parse_paragraph(const Context &context,
                     const ElementIdentifier paragraph_id,
                     std::string_view content) {
  ElementRegistry &registry = *context.registry;
  Budget &budget = *context.budget;

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

/// The drawable each `U+FFFC` in a storage anchors there, as byte offsets into
/// @p text.
std::vector<std::pair<std::size_t, std::uint64_t>>
read_attachments(const Message &storage, const std::string_view text) {
  std::vector<std::uint64_t> indices;
  std::vector<std::uint64_t> identifiers;

  if (const std::optional<std::string_view> table =
          storage.bytes_field(text_storage::attachments);
      table.has_value()) {
    for (const Field &entry :
         Message(*table).repeated_field(attribute_table::entries)) {
      if (entry.type != WireType::length_delimited) {
        throw std::runtime_error("iwork: malformed attachment table");
      }
      const Message run(entry.bytes);
      const std::optional<std::uint64_t> identifier =
          reference_identifier(run, attribute_table_entry::object);
      if (!identifier.has_value()) {
        continue;
      }
      indices.push_back(
          run.number_field(attribute_table_entry::character_index).value_or(0));
      identifiers.push_back(*identifier);
    }
  }

  const std::vector<std::size_t> offsets =
      util::string::utf16_offsets(text, indices);

  std::vector<std::pair<std::size_t, std::uint64_t>> result;
  result.reserve(offsets.size());
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    result.emplace_back(offsets[i], identifiers[i]);
  }
  return result;
}

/// The drawable an attachment stands for, where it is one we read.
void parse_attachment(const Context &context, const ElementIdentifier parent_id,
                      const std::uint64_t identifier) {
  const Object &attachment = context.package->object(identifier);
  if (attachment.type != archive_type::drawable_attachment) {
    // a table of contents, a footnote mark, an inline shape — none read yet
    return;
  }
  const std::optional<std::uint64_t> drawable_identifier = reference_identifier(
      Message(attachment.payload), attachment_archive::drawable);
  if (!drawable_identifier.has_value()) {
    return;
  }
  const Object &drawable = context.package->object(*drawable_identifier);
  if (drawable.type != archive_type::table_info) {
    return;
  }
  parse_table(context.deeper(), parent_id, *drawable_identifier);
}

/// Fills a `Table` element from the tile reader's view of it.
void parse_table(const Context &context, const ElementIdentifier parent_id,
                 const std::uint64_t identifier) {
  ElementRegistry &registry = *context.registry;
  Budget &budget = *context.budget;
  const TableModel &model =
      context.tables->table(*context.package, *context.budget, identifier);

  budget.spend_element();
  auto [table_id, element, payload] = registry.create_table_element();
  payload.rows = model.rows;
  payload.columns = model.columns;
  registry.append_child(parent_id, table_id);

  // the tiles are sparse and the rows below are dense, so index them once
  // rather than searching the list per position
  std::map<std::pair<std::uint32_t, std::uint32_t>, const TableModel::Cell *>
      by_position;
  for (const TableModel::Cell &cell : model.cells) {
    by_position.emplace(std::pair(cell.row, cell.column), &cell);
  }

  for (std::uint32_t column = 0; column < model.columns; ++column) {
    budget.spend_element();
    auto [column_id, column_element] =
        registry.create_element(ElementType::table_column);
    registry.append_table_column(table_id, column_id);
  }

  // a table's rows are dense — the renderer walks them rather than asking by
  // coordinate, so a row the tiles do not carry is a row of empty cells. The
  // extent is the file's word, so every position is spent against the budget
  // rather than trusted to be one an app wrote.
  for (std::uint32_t row = 0; row < model.rows; ++row) {
    budget.spend_element();
    auto [row_id, row_element] =
        registry.create_element(ElementType::table_row);
    registry.append_child(table_id, row_id);

    for (std::uint32_t column = 0; column < model.columns; ++column) {
      budget.spend_element();
      auto [cell_id, cell_element, cell] =
          registry.create_cell_element(ElementType::table_cell);
      cell.row = row;
      cell.column = column;
      registry.append_child(row_id, cell_id);

      const auto it = by_position.find({row, column});
      if (it != by_position.end()) {
        cell.value_type = it->second->value_type;
        fill_cell(context, cell_id, *it->second);
      }
    }
  }
}

} // namespace

} // namespace odr::internal::iwork

namespace odr::internal {

void iwork::parse_storage(const Context &context,
                          const ElementIdentifier parent_id,
                          const Message &storage) {
  ElementRegistry &registry = *context.registry;
  Budget &budget = *context.budget;

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
  const std::vector<std::pair<std::size_t, std::uint64_t>> attachments =
      read_attachments(storage, text);

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
    parse_paragraph(context, paragraph_id, content);

    // a drawable goes after the paragraph its anchor sits in: the anchor is
    // an inline character but a table is not something a paragraph can hold
    for (const auto &[offset, identifier] : attachments) {
      if (offset >= begin && offset < end) {
        parse_attachment(context, parent_id, identifier);
      }
    }
  }
}

void iwork::fill_cell(const Context &context, const ElementIdentifier cell_id,
                      const TableModel::Cell &cell) {
  ElementRegistry &registry = *context.registry;
  Budget &budget = *context.budget;

  if (cell.storage_identifier.has_value()) {
    const Object &payload = context.package->object(*cell.storage_identifier);
    if (payload.type != archive_type::rich_text_payload) {
      return;
    }
    const std::optional<std::uint64_t> storage_identifier =
        reference_identifier(Message(payload.payload),
                             rich_text_payload::storage);
    if (!storage_identifier.has_value()) {
      return;
    }
    const Object &storage = context.package->object(*storage_identifier);
    if (storage.type != archive_type::text_storage) {
      return;
    }
    parse_storage(context.deeper(), cell_id, Message(storage.payload));
    return;
  }

  if (cell.text.empty()) {
    return;
  }
  budget.spend_element();
  auto [paragraph_id, paragraph] =
      registry.create_element(ElementType::paragraph);
  registry.append_child(cell_id, paragraph_id);

  budget.spend_element();
  budget.spend_text(cell.text.size());
  auto [text_id, element, text] = registry.create_text_element();
  text.text = cell.text;
  registry.append_child(paragraph_id, text_id);
}

} // namespace odr::internal
