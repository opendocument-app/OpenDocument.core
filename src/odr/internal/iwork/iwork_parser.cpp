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

/// The object a `TSP.Reference` in field @p number names.
std::optional<std::uint64_t> reference_identifier(const Message &message,
                                                  const std::uint32_t number) {
  const std::optional<std::string_view> bytes = message.bytes_field(number);
  if (!bytes.has_value()) {
    return {};
  }
  return Message(*bytes).number_field(reference::identifier);
}

/// The objects the repeated `TSP.Reference` field @p number names, in order.
std::vector<std::uint64_t> reference_identifiers(const Message &message,
                                                 const std::uint32_t number) {
  std::vector<std::uint64_t> result;
  for (const Field &field : message.repeated_field(number)) {
    if (field.type != WireType::length_delimited) {
      throw std::runtime_error("iwork: malformed reference");
    }
    if (const std::optional<std::uint64_t> identifier =
            Message(field.bytes).number_field(reference::identifier);
        identifier.has_value()) {
      result.push_back(*identifier);
    }
  }
  return result;
}

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
void parse_paragraph(ElementRegistry &registry,
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

/// Appends the paragraphs of a `TSWP.StorageArchive` to @p parent_id. Shared
/// by every place text lives: a Pages body, a Keynote text box, a table cell.
void parse_storage(ElementRegistry &registry, const ElementIdentifier parent_id,
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

    auto [paragraph_id, paragraph] =
        registry.create_element(ElementType::paragraph);
    registry.append_child(parent_id, paragraph_id);
    parse_paragraph(registry, paragraph_id, content);
  }
}

/// The root archive of the package's `Document` component, checked against the
/// type the app is expected to write.
const Object &root_archive(Package &package, const std::uint32_t type) {
  const std::vector<Object> &objects = package.component("Document").objects();
  if (objects.empty() || objects.front().type != type) {
    throw std::runtime_error("iwork: no document archive");
  }
  return objects.front();
}

/// A `TSP.Point`, which iWork writes as two `float`s in points.
std::optional<ElementRegistry::Size> read_size(const Message &message,
                                               const std::uint32_t number) {
  const std::optional<std::string_view> bytes = message.bytes_field(number);
  if (!bytes.has_value()) {
    return {};
  }
  const Message size(*bytes);
  const std::optional<float> width = size.float_field(point::x);
  const std::optional<float> height = size.float_field(point::y);
  if (!width.has_value() || !height.has_value()) {
    return {};
  }
  return ElementRegistry::Size{*width, *height};
}

/// Where a `TSD.ShapeArchive` sits on its page, in points.
std::optional<ElementRegistry::Rect> shape_rect(const Message &shape) {
  const std::optional<std::string_view> drawable =
      shape.bytes_field(shape_archive::drawable);
  if (!drawable.has_value()) {
    return {};
  }
  const Message drawable_message(*drawable);
  const std::optional<std::string_view> geometry =
      drawable_message.bytes_field(drawable_archive::geometry);
  if (!geometry.has_value()) {
    return {};
  }
  const Message geometry_message(*geometry);

  const std::optional<ElementRegistry::Size> position =
      read_size(geometry_message, geometry_archive::position);
  const std::optional<ElementRegistry::Size> size =
      read_size(geometry_message, geometry_archive::size);
  if (!position.has_value()) {
    return {};
  }
  return ElementRegistry::Rect{position->width, position->height,
                               size.has_value() ? size->width : 0.0F,
                               size.has_value() ? size->height : 0.0F};
}

/// The `TSWP.ShapeArchive` a drawable holds, or nothing when it is a kind we
/// do not read — a table, an image, a group. An unmapped drawable is skipped
/// rather than thrown on: there is no spec, so it is a shape Apple ships and
/// we have not seen.
std::optional<Message> text_shape_of(const Object &drawable) {
  switch (drawable.type) {
  case archive_type::text_shape:
    return Message(drawable.payload);
  case archive_type::keynote_placeholder: {
    const Message placeholder(drawable.payload);
    const std::optional<std::string_view> shape =
        placeholder.bytes_field(placeholder_archive::shape);
    if (!shape.has_value()) {
      return {};
    }
    return Message(*shape);
  }
  default:
    return {};
  }
}

/// Appends one slide's text boxes to @p slide_id as frames.
void parse_slide(ElementRegistry &registry, Package &package,
                 const ElementIdentifier slide_id, const Object &slide) {
  const Message slide_message(slide.payload);

  for (const std::uint64_t identifier :
       reference_identifiers(slide_message, slide_archive::drawables)) {
    const Object &drawable = package.object(identifier);
    const std::optional<Message> shape = text_shape_of(drawable);
    if (!shape.has_value()) {
      continue;
    }

    const std::optional<std::uint64_t> storage_identifier =
        reference_identifier(*shape, text_shape::storage);
    if (!storage_identifier.has_value()) {
      continue;
    }
    const Object &storage = package.object(*storage_identifier);
    if (storage.type != archive_type::text_storage) {
      continue;
    }

    auto [frame_id, frame, payload] = registry.create_frame_element();
    if (const std::optional<std::string_view> inner =
            shape->bytes_field(text_shape::shape);
        inner.has_value()) {
      payload.rect = shape_rect(Message(*inner));
    }
    registry.append_child(slide_id, frame_id);

    parse_storage(registry, frame_id, Message(storage.payload));
  }
}

} // namespace

} // namespace odr::internal::iwork

namespace odr::internal {

ElementIdentifier
iwork::parse_pages_tree(ElementRegistry &registry,
                        const abstract::ReadableFilesystem &files) {
  Package package(files);

  const Message document(
      root_archive(package, archive_type::pages_document).payload);
  const std::optional<std::uint64_t> body_identifier =
      reference_identifier(document, document_archive::body_storage);
  if (!body_identifier.has_value()) {
    throw std::runtime_error("iwork: document archive holds no body");
  }

  const Object &body_object = package.object(*body_identifier);
  if (body_object.type != archive_type::text_storage) {
    throw std::runtime_error("iwork: body is not a text storage");
  }

  auto [root_id, root] = registry.create_element(ElementType::root);
  parse_storage(registry, root_id, Message(body_object.payload));
  return root_id;
}

ElementIdentifier
iwork::parse_keynote_tree(ElementRegistry &registry,
                          const abstract::ReadableFilesystem &files) {
  Package package(files);

  const Message document(
      root_archive(package, archive_type::app_document).payload);
  const std::optional<std::uint64_t> show_identifier =
      reference_identifier(document, document_archive::show);
  if (!show_identifier.has_value()) {
    throw std::runtime_error("iwork: document archive holds no show");
  }

  const Object &show_object = package.object(*show_identifier);
  if (show_object.type != archive_type::keynote_show) {
    throw std::runtime_error("iwork: show is not a show archive");
  }
  const Message show(show_object.payload);
  const std::optional<ElementRegistry::Size> slide_size =
      read_size(show, show_archive::size);

  auto [root_id, root] = registry.create_element(ElementType::root);

  const std::optional<std::string_view> tree =
      show.bytes_field(show_archive::slide_tree);
  if (!tree.has_value()) {
    return root_id;
  }

  std::size_t number = 0;
  for (const std::uint64_t identifier :
       reference_identifiers(Message(*tree), slide_tree::nodes)) {
    const Object &node = package.object(identifier);
    if (node.type != archive_type::keynote_slide_node) {
      continue;
    }
    const std::optional<std::uint64_t> slide_identifier =
        reference_identifier(Message(node.payload), slide_node::slide);
    if (!slide_identifier.has_value()) {
      continue;
    }
    const Object &slide = package.object(*slide_identifier);
    if (slide.type != archive_type::keynote_slide) {
      continue;
    }

    auto [slide_id, element, payload] = registry.create_slide_element();
    // slides carry no name in the archive; number them in presentation order
    payload.name = "Slide " + std::to_string(++number);
    payload.size = slide_size;
    registry.append_child(root_id, slide_id);

    parse_slide(registry, package, slide_id, slide);
  }

  return root_id;
}

} // namespace odr::internal
