#include <odr/internal/iwork/iwork_parser.hpp>

#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_budget.hpp>
#include <odr/internal/iwork/iwork_element_registry.hpp>
#include <odr/internal/iwork/iwork_protobuf.hpp>
#include <odr/internal/iwork/iwork_table.hpp>
#include <odr/internal/iwork/iwork_text.hpp>
#include <odr/internal/iwork/iwork_types.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::iwork {

namespace {

/// The root archive of the package's `Document` component, checked against the
/// type the app is expected to write.
const Object &root_archive(Package &package, const std::uint32_t type) {
  const std::vector<Object> &objects = package.component("Document").objects();
  if (objects.empty() || objects.front().type != type) {
    throw std::runtime_error("iwork: no document archive");
  }
  return objects.front();
}

/// A `TSP.Point`: two `float`s in points, which is how iWork writes a size
/// too.
struct Point final {
  float x{};
  float y{};
};

std::optional<Point> read_point(const Message &message,
                                const std::uint32_t number) {
  const std::optional<std::string_view> bytes = message.bytes_field(number);
  if (!bytes.has_value()) {
    return {};
  }
  const Message point_message(*bytes);
  const std::optional<float> x = point_message.float_field(point::x);
  const std::optional<float> y = point_message.float_field(point::y);
  if (!x.has_value() || !y.has_value()) {
    return {};
  }
  return Point{*x, *y};
}

/// The `TSP.Point` in field @p number, read as the size it stands for.
std::optional<ElementRegistry::Size> read_size(const Message &message,
                                               const std::uint32_t number) {
  const std::optional<Point> size = read_point(message, number);
  if (!size.has_value()) {
    return {};
  }
  return ElementRegistry::Size{size->x, size->y};
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

  const std::optional<Point> position =
      read_point(geometry_message, geometry_archive::position);
  if (!position.has_value()) {
    return {};
  }

  ElementRegistry::Rect rect{.x = position->x, .y = position->y};
  if (const std::optional<Point> size =
          read_point(geometry_message, geometry_archive::size);
      size.has_value()) {
    // a zero side is a box that grows with its text, not a box of zero extent
    if (size->x != 0) {
      rect.width = size->x;
    }
    if (size->y != 0) {
      rect.height = size->y;
    }
  }
  return rect;
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
void parse_slide(const Context &context, const ElementIdentifier slide_id,
                 const Object &slide) {
  ElementRegistry &registry = *context.registry;
  Package &package = *context.package;
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

    context.budget->spend_element();
    auto [frame_id, frame, payload] = registry.create_frame_element();
    if (const std::optional<std::string_view> inner =
            shape->bytes_field(text_shape::shape);
        inner.has_value()) {
      payload.rect = shape_rect(Message(*inner));
    }
    registry.append_child(slide_id, frame_id);

    parse_storage(context, frame_id, Message(storage.payload));
  }
}

/// One Numbers sheet holds many tables and our `Sheet` is one grid, so each
/// table becomes an odr sheet of its own — taking only the first would drop
/// data with nothing to show for it.
void parse_sheet(const Context &context, const ElementIdentifier root_id,
                 const Object &sheet) {
  ElementRegistry &registry = *context.registry;
  Budget &budget = *context.budget;
  const Message sheet_message(sheet.payload);
  const std::string name =
      std::string(sheet_message.bytes_field(sheet_archive::name)
                      .value_or(std::string_view()));

  for (const std::uint64_t identifier :
       reference_identifiers(sheet_message, sheet_archive::drawables)) {
    const Object &drawable = context.package->object(identifier);
    if (drawable.type != archive_type::table_info) {
      // a chart, a text box or an image on the sheet; none read yet
      continue;
    }
    const TableModel model = read_table(*context.package, identifier);

    budget.spend_element();
    auto [sheet_id, element, payload] = registry.create_sheet_element();
    payload.name = model.name.empty() ? name : name + " – " + model.name;
    payload.rows = model.rows;
    payload.columns = model.columns;
    registry.append_child(root_id, sheet_id);

    for (const TableModel::Cell &cell : model.cells) {
      budget.spend_element();
      auto [cell_id, cell_element, entry] =
          registry.create_cell_element(ElementType::sheet_cell);
      entry.row = cell.row;
      entry.column = cell.column;
      entry.value_type = cell.value_type;
      registry.append_sheet_cell(sheet_id, cell_id);

      fill_cell(context, cell_id, cell);

      payload.content_rows = std::max(payload.content_rows, cell.row + 1);
      payload.content_columns =
          std::max(payload.content_columns, cell.column + 1);
    }
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

  Budget budget;
  auto [root_id, root] = registry.create_element(ElementType::root);
  parse_storage({&registry, &package, &budget, 0}, root_id,
                Message(body_object.payload));
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

  Budget budget;
  auto [root_id, root] = registry.create_element(ElementType::root);

  // a show that carries no slide tree is a deck with no slides
  const Message tree(
      show.bytes_field(show_archive::slide_tree).value_or(std::string_view()));

  std::size_t number = 0;
  for (const std::uint64_t identifier :
       reference_identifiers(tree, slide_tree::nodes)) {
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

    budget.spend_element();
    auto [slide_id, element, payload] = registry.create_slide_element();
    // slides carry no name in the archive; number them in presentation order
    payload.name = "Slide " + std::to_string(++number);
    payload.size = slide_size;
    registry.append_child(root_id, slide_id);

    parse_slide({&registry, &package, &budget, 0}, slide_id, slide);
  }

  return root_id;
}

ElementIdentifier
iwork::parse_numbers_tree(ElementRegistry &registry,
                          const abstract::ReadableFilesystem &files) {
  Package package(files);

  const Message document(
      root_archive(package, archive_type::app_document).payload);

  Budget budget;
  auto [root_id, root] = registry.create_element(ElementType::root);

  for (const std::uint64_t identifier :
       reference_identifiers(document, document_archive::sheets)) {
    const Object &sheet = package.object(identifier);
    if (sheet.type != archive_type::numbers_sheet) {
      continue;
    }
    parse_sheet({&registry, &package, &budget, 0}, root_id, sheet);
  }

  return root_id;
}

} // namespace odr::internal
