#include <odr/internal/iwork/iwork_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/iwork/iwork_parser.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace odr::internal::iwork {

namespace {

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(ElementRegistry &registry);

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const FileType file_type,
                             const abstract::ReadableFilesystem &files) {
  switch (file_type) {
  case FileType::iwork_pages:
    return parse_pages_tree(registry, files);
  case FileType::iwork_keynote:
    return parse_keynote_tree(registry, files);
  case FileType::iwork_numbers:
    return parse_numbers_tree(registry, files);
  default:
    throw UnsupportedFileType(file_type);
  }
}

} // namespace

Document::Document(const FileType file_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(file_type, document_type_by_file_type(file_type),
                         std::move(files)) {
  m_root_element = parse_tree(m_element_registry, file_type, *m_files);

  m_element_adapter = create_element_adapter(m_element_registry);
}

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

namespace {

using AdapterBase = internal::RegistryElementAdapter<
    ElementRegistry, abstract::TextRootAdapter, abstract::SlideAdapter,
    abstract::SheetAdapter, abstract::SheetCellAdapter, abstract::TableAdapter,
    abstract::TableColumnAdapter, abstract::TableRowAdapter,
    abstract::TableCellAdapter, abstract::FrameAdapter,
    abstract::LineBreakAdapter, abstract::ParagraphAdapter,
    abstract::TextAdapter>;

class ElementAdapter final : public AdapterBase {
public:
  explicit ElementAdapter(ElementRegistry &registry) : AdapterBase(registry) {}

  // The page geometry sits in the document archive and the styles in
  // `Index/DocumentStylesheet.iwa`; neither is read yet.
  [[nodiscard]] PageLayout text_root_page_layout(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] std::string
  slide_name(const ElementIdentifier element_id) const override {
    return m_registry->slide_element_at(element_id).name;
  }
  [[nodiscard]] PageLayout
  slide_page_layout(const ElementIdentifier element_id) const override {
    const std::optional<ElementRegistry::Size> &size =
        m_registry->slide_element_at(element_id).size;
    if (!size.has_value()) {
      return {};
    }
    return {
        .width = points(size->width),
        .height = points(size->height),
        .print_orientation = {},
        .margin = {},
        .background_color = {},
        .direction = {},
    };
  }
  [[nodiscard]] ElementIdentifier slide_master_page(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // `Index/TemplateSlide-*.iwa` holds the masters; nothing reads them yet.
    return null_element_id;
  }

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).name;
  }
  /// TODO the print setup of a `.numbers` sheet is not read.
  [[nodiscard]] PageLayout sheet_page_layout(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    const ElementRegistry::Sheet &sheet =
        m_registry->sheet_element_at(element_id);
    return {sheet.rows, sheet.columns};
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    const ElementRegistry::Sheet &sheet =
        m_registry->sheet_element_at(element_id);
    if (!range.has_value()) {
      return {sheet.content_rows, sheet.content_columns};
    }
    return {std::min(sheet.content_rows, range->rows),
            std::min(sheet.content_columns, range->columns)};
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell(const ElementIdentifier element_id, const std::uint32_t column,
             const std::uint32_t row) const override {
    return m_registry->sheet_element_at(element_id).cell(column, row);
  }
  [[nodiscard]] ElementIdentifier sheet_first_shape(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // a chart or a text box on the sheet; none read yet
    return null_element_id;
  }
  [[nodiscard]] TableStyle sheet_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TableColumnStyle sheet_column_style(
      [[maybe_unused]] const ElementIdentifier element_id,
      [[maybe_unused]] const std::uint32_t column) const override {
    return {};
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style([[maybe_unused]] const ElementIdentifier element_id,
                  [[maybe_unused]] const std::uint32_t row) const override {
    return {};
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::uint32_t column,
                   [[maybe_unused]] const std::uint32_t row) const override {
    return {};
  }

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    const ElementRegistry::Cell &cell = m_registry->cell_element_at(element_id);
    // `TablePosition` is (column, row); `TableDimensions` is (rows, columns)
    return TablePosition(cell.column, cell.row);
  }
  [[nodiscard]] bool sheet_cell_is_covered(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return false;
  }
  [[nodiscard]] TableDimensions sheet_cell_span(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // merged ranges are recorded outside the tiles and are not read yet
    return {1, 1};
  }
  [[nodiscard]] ValueType
  sheet_cell_value_type(const ElementIdentifier element_id) const override {
    return m_registry->cell_element_at(element_id).value_type;
  }
  /// A number stays the decimal the file states, which is the cell's text.
  /// Formulas live in `CalculationEngine`, which is not read.
  [[nodiscard]] CellValue
  sheet_cell_value(const ElementIdentifier element_id) const override {
    CellValue result;
    result.type = m_registry->cell_element_at(element_id).value_type;
    return result;
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ElementIdentifier element_id) const override {
    const ElementRegistry::Table &table =
        m_registry->table_element_at(element_id);
    return {table.rows, table.columns};
  }
  [[nodiscard]] ElementIdentifier
  table_first_column(const ElementIdentifier element_id) const override {
    return m_registry->table_element_at(element_id).first_column_id;
  }
  [[nodiscard]] ElementIdentifier
  table_first_row(const ElementIdentifier element_id) const override {
    return element_first_child(element_id);
  }
  [[nodiscard]] TableStyle table_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TableColumnStyle table_column_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TableRowStyle table_row_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] bool table_cell_is_covered(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return false;
  }
  [[nodiscard]] TableDimensions table_cell_span(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {1, 1};
  }
  [[nodiscard]] ValueType
  table_cell_value_type(const ElementIdentifier element_id) const override {
    return m_registry->cell_element_at(element_id).value_type;
  }
  [[nodiscard]] TableCellStyle table_cell_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] AnchorType frame_anchor_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<Measure>
  frame_x(const ElementIdentifier element_id) const override {
    return rect_measure(element_id, [](const ElementRegistry::Rect &r) {
      return std::optional(r.x);
    });
  }
  [[nodiscard]] std::optional<Measure>
  frame_y(const ElementIdentifier element_id) const override {
    return rect_measure(element_id, [](const ElementRegistry::Rect &r) {
      return std::optional(r.y);
    });
  }
  [[nodiscard]] std::optional<Measure>
  frame_width(const ElementIdentifier element_id) const override {
    return rect_measure(element_id,
                        [](const ElementRegistry::Rect &r) { return r.width; });
  }
  [[nodiscard]] std::optional<Measure>
  frame_height(const ElementIdentifier element_id) const override {
    return rect_measure(
        element_id, [](const ElementRegistry::Rect &r) { return r.height; });
  }
  [[nodiscard]] std::optional<std::int32_t> frame_z_index(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return std::nullopt;
  }
  [[nodiscard]] std::optional<DrawingTransform> frame_transform(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle frame_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] TextStyle line_break_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] ParagraphStyle paragraph_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] TextStyle paragraph_text_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    return m_registry->text_element_at(element_id).text;
  }
  void
  text_set_content([[maybe_unused]] const ElementIdentifier element_id,
                   [[maybe_unused]] const std::string &text) const override {
    throw UnsupportedOperation();
  }
  [[nodiscard]] TextStyle text_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

private:
  static Measure points(const float value) {
    return Measure(value, DynamicUnit("pt"));
  }

  /// One side of a frame's rectangle, or nothing where the frame has no
  /// geometry or the side itself is absent — which only an extent is, for a
  /// box that grows with its text.
  template <typename Selector>
  [[nodiscard]] std::optional<Measure>
  rect_measure(const ElementIdentifier element_id,
               const Selector &select) const {
    const std::optional<ElementRegistry::Rect> &rect =
        m_registry->frame_element_at(element_id).rect;
    if (!rect.has_value()) {
      return std::nullopt;
    }
    const std::optional<float> value = select(*rect);
    if (!value.has_value()) {
      return std::nullopt;
    }
    return points(*value);
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(registry);
}

} // namespace

} // namespace odr::internal::iwork
