#include <odr/internal/iwork/iwork_parser.hpp>

#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_element_registry.hpp>
#include <odr/internal/iwork/iwork_protobuf.hpp>
#include <odr/internal/iwork/iwork_types.hpp>
#include <odr/internal/util/string_util.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal {

namespace {

/// `U+2028 LINE SEPARATOR` — a line break inside a paragraph.
constexpr std::string_view line_separator = "\xe2\x80\xa8";
/// `U+FFFC OBJECT REPLACEMENT CHARACTER` — where a drawable is anchored in the
/// text. Nothing reads drawables yet, so the anchor is dropped rather than
/// rendered as a glyph.
constexpr std::string_view object_replacement = "\xef\xbf\xbc";

/// The character index each paragraph of @p storage starts at. Paragraph
/// boundaries are the run table's rather than every `\n` in the text — the two
/// agree today, but the table is what says so.
std::vector<std::uint64_t> paragraph_starts(const iwork::Message &storage) {
  std::vector<std::uint64_t> result;

  if (const std::optional<std::string_view> table =
          storage.bytes_field(iwork::text_storage::paragraph_styles);
      table.has_value()) {
    for (const iwork::Field &entry : iwork::Message(*table).repeated_field(
             iwork::attribute_table::entries)) {
      if (entry.type != iwork::WireType::length_delimited) {
        throw std::runtime_error("iwork: malformed paragraph style table");
      }
      const iwork::Message run(entry.bytes);
      result.push_back(
          run.number_field(iwork::attribute_table_entry::character_index)
              .value_or(0));
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
void parse_paragraph(iwork::ElementRegistry &registry,
                     const ElementIdentifier paragraph_id,
                     std::string_view content) {
  const auto append_text = [&](const std::string_view part) {
    std::string text(part);
    util::string::replace_all(text, std::string(object_replacement), "");
    if (text.empty()) {
      return;
    }
    auto [text_id, element, payload] = registry.create_text_element();
    payload.text = std::move(text);
    registry.append_child(paragraph_id, text_id);
  };

  for (std::size_t position = content.find(line_separator);
       position != std::string_view::npos;
       position = content.find(line_separator)) {
    append_text(content.substr(0, position));

    auto [break_id, element] = registry.create_element(ElementType::line_break);
    registry.append_child(paragraph_id, break_id);

    content.remove_prefix(position + line_separator.size());
  }
  append_text(content);
}

} // namespace

ElementIdentifier
iwork::parse_pages_tree(ElementRegistry &registry,
                        const abstract::ReadableFilesystem &files) {
  Package package(files);

  const std::vector<Object> &objects = package.component("Document").objects();
  if (objects.empty() || objects.front().type != archive_type::pages_document) {
    throw std::runtime_error("iwork: no pages document archive");
  }

  const Message document(objects.front().payload);
  const std::optional<std::string_view> body =
      document.bytes_field(document_archive::body_storage);
  if (!body.has_value()) {
    throw std::runtime_error("iwork: document archive holds no body");
  }
  const std::optional<std::uint64_t> body_identifier =
      Message(*body).number_field(reference::identifier);
  if (!body_identifier.has_value()) {
    throw std::runtime_error("iwork: body reference names no object");
  }

  const Object &body_object = package.object(*body_identifier);
  if (body_object.type != archive_type::text_storage) {
    throw std::runtime_error("iwork: body is not a text storage");
  }
  const Message storage(body_object.payload);

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

  auto [root_id, root] = registry.create_element(ElementType::root);

  const std::string_view body_text(text);
  for (std::size_t i = 0; i < starts.size(); ++i) {
    const std::size_t begin = starts[i];
    const std::size_t end = i + 1 < starts.size() ? starts[i + 1] : text.size();

    std::string_view content = body_text.substr(begin, end - begin);
    // the paragraph mark belongs to the paragraph it ends, and the last
    // paragraph of a body does not carry one
    if (content.ends_with('\n')) {
      content.remove_suffix(1);
    }

    auto [paragraph_id, paragraph] =
        registry.create_element(ElementType::paragraph);
    registry.append_child(root_id, paragraph_id);
    parse_paragraph(registry, paragraph_id, content);
  }

  return root_id;
}

} // namespace odr::internal
