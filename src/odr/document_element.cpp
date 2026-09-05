#include <odr/document_element.hpp>

#include <odr/document_path.hpp>
#include <odr/file.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

#include <odr/internal/abstract/document.hpp>

namespace odr {

Element::Element() = default;

Element::Element(const internal::abstract::ElementAdapter *adapter,
                 const ElementIdentifier identifier)
    : m_adapter{adapter}, m_identifier{identifier} {}

Element::operator bool() const { return exists_(); }

bool Element::exists_() const {
  return m_adapter != nullptr && m_identifier != null_element_id;
}

ElementType Element::type() const {
  return exists_() ? m_adapter->element_type(m_identifier) : ElementType::none;
}

Element Element::parent() const {
  return exists_() ? Element(m_adapter, m_adapter->element_parent(m_identifier))
                   : Element();
}

Element Element::first_child() const {
  return exists_()
             ? Element(m_adapter, m_adapter->element_first_child(m_identifier))
             : Element();
}

Element Element::previous_sibling() const {
  return exists_() ? Element(m_adapter,
                             m_adapter->element_previous_sibling(m_identifier))
                   : Element();
}

Element Element::next_sibling() const {
  return exists_()
             ? Element(m_adapter, m_adapter->element_next_sibling(m_identifier))
             : Element();
}

bool Element::is_unique() const {
  return exists_() ? m_adapter->element_is_unique(m_identifier) : false;
}

bool Element::is_self_locatable() const {
  return exists_() ? m_adapter->element_is_self_locatable(m_identifier) : false;
}

bool Element::is_editable() const {
  return exists_() ? m_adapter->element_is_editable(m_identifier) : false;
}

DocumentPath Element::document_path() const {
  return exists_() ? m_adapter->element_document_path(m_identifier)
                   : DocumentPath();
}

Element Element::navigate_path(const DocumentPath &path) const {
  return exists_()
             ? Element(m_adapter,
                       m_adapter->element_navigate_path(m_identifier, path))
             : Element();
}

TextRoot Element::as_text_root() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->text_root_adapter(m_identifier)};
}

Slide Element::as_slide() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->slide_adapter(m_identifier)};
}

Sheet Element::as_sheet() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->sheet_adapter(m_identifier)};
}

Page Element::as_page() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->page_adapter(m_identifier)};
}

SheetCell Element::as_sheet_cell() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->sheet_cell_adapter(m_identifier)};
}

MasterPage Element::as_master_page() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier,
          m_adapter->master_page_adapter(m_identifier)};
}

LineBreak Element::as_line_break() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->line_break_adapter(m_identifier)};
}

Paragraph Element::as_paragraph() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->paragraph_adapter(m_identifier)};
}

Span Element::as_span() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->span_adapter(m_identifier)};
}

Text Element::as_text() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->text_adapter(m_identifier)};
}

Link Element::as_link() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->link_adapter(m_identifier)};
}

Bookmark Element::as_bookmark() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->bookmark_adapter(m_identifier)};
}

List Element::as_list() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->list_adapter(m_identifier)};
}

ListItem Element::as_list_item() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->list_item_adapter(m_identifier)};
}

Table Element::as_table() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->table_adapter(m_identifier)};
}

TableColumn Element::as_table_column() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier,
          m_adapter->table_column_adapter(m_identifier)};
}

TableRow Element::as_table_row() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->table_row_adapter(m_identifier)};
}

TableCell Element::as_table_cell() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->table_cell_adapter(m_identifier)};
}

Frame Element::as_frame() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->frame_adapter(m_identifier)};
}

Rect Element::as_rect() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->rect_adapter(m_identifier)};
}

Line Element::as_line() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->line_adapter(m_identifier)};
}

Circle Element::as_circle() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->circle_adapter(m_identifier)};
}

CustomShape Element::as_custom_shape() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier,
          m_adapter->custom_shape_adapter(m_identifier)};
}

Image Element::as_image() const {
  if (!exists_()) {
    return {};
  }
  return {m_adapter, m_identifier, m_adapter->image_adapter(m_identifier)};
}

ElementRange Element::children() const {
  if (!exists_()) {
    return {};
  }
  return ElementRange(
      ElementIterator(m_adapter, m_adapter->element_first_child(m_identifier)));
}

ElementIterator::ElementIterator() = default;

ElementIterator::ElementIterator(
    const internal::abstract::ElementAdapter *adapter,
    const ElementIdentifier identifier)
    : m_adapter{adapter}, m_identifier{identifier} {}

Element ElementIterator::operator*() const { return {m_adapter, m_identifier}; }

ElementIterator &ElementIterator::operator++() {
  if (exists_()) {
    m_identifier = m_adapter->element_next_sibling(m_identifier);
  }
  return *this;
}

ElementIterator ElementIterator::operator++(int) {
  ElementIterator result = *this;
  ++*this;
  return result;
}

bool ElementIterator::exists_() const {
  return m_adapter != nullptr && m_identifier != null_element_id;
}

ElementRange::ElementRange() = default;

ElementRange::ElementRange(const ElementIterator &begin) : m_begin{begin} {}

ElementRange::ElementRange(const ElementIterator &begin,
                           const ElementIterator &end)
    : m_begin{begin}, m_end{end} {}

ElementIterator ElementRange::begin() const { return m_begin; }

ElementIterator ElementRange::end() const { return m_end; }

PageLayout TextRoot::page_layout() const {
  return exists_() ? m_adapter2->text_root_page_layout(m_identifier)
                   : PageLayout();
}

MasterPage TextRoot::first_master_page() const {
  if (!exists_()) {
    return {};
  }
  const ElementIdentifier master_page_id =
      m_adapter2->text_root_first_master_page(m_identifier);
  return {m_adapter, master_page_id,
          m_adapter->master_page_adapter(master_page_id)};
}

std::string Slide::name() const {
  return exists_() ? m_adapter2->slide_name(m_identifier) : "";
}

PageLayout Slide::page_layout() const {
  return exists_() ? m_adapter2->slide_page_layout(m_identifier) : PageLayout();
}

MasterPage Slide::master_page() const {
  if (!exists_()) {
    return {};
  }
  const ElementIdentifier master_page_id =
      m_adapter2->slide_master_page(m_identifier);
  return {m_adapter, master_page_id,
          m_adapter->master_page_adapter(master_page_id)};
}

std::string Sheet::name() const {
  return exists_() ? m_adapter2->sheet_name(m_identifier) : "";
}

PageLayout Sheet::page_layout() const {
  return exists_() ? m_adapter2->sheet_page_layout(m_identifier) : PageLayout();
}

TableDimensions Sheet::dimensions() const {
  return exists_() ? m_adapter2->sheet_dimensions(m_identifier)
                   : TableDimensions();
}

TableDimensions
Sheet::content(const std::optional<TableDimensions> range) const {
  return exists_() ? m_adapter2->sheet_content(m_identifier, range)
                   : TableDimensions();
}

SheetCell Sheet::cell(const std::uint32_t column,
                      const std::uint32_t row) const {
  if (!exists_()) {
    return {};
  }
  const ElementIdentifier cell_id =
      m_adapter2->sheet_cell(m_identifier, column, row);
  if (cell_id == null_element_id) {
    return {};
  }
  return {m_adapter, cell_id, m_adapter->sheet_cell_adapter(cell_id)};
}

ElementRange Sheet::shapes() const {
  if (!exists_()) {
    return {};
  }
  const ElementIdentifier first_shape_id =
      m_adapter2->sheet_first_shape(m_identifier);
  return ElementRange(ElementIterator(m_adapter, first_shape_id));
}

TableStyle Sheet::style() const {
  return exists_() ? m_adapter2->sheet_style(m_identifier) : TableStyle();
}

TableColumnStyle Sheet::column_style(const std::uint32_t column) const {
  return exists_() ? m_adapter2->sheet_column_style(m_identifier, column)
                   : TableColumnStyle();
}

TableRowStyle Sheet::row_style(const std::uint32_t row) const {
  return exists_() ? m_adapter2->sheet_row_style(m_identifier, row)
                   : TableRowStyle();
}

TableCellStyle Sheet::cell_style(const std::uint32_t column,
                                 const std::uint32_t row) const {
  return exists_() ? m_adapter2->sheet_cell_style(m_identifier, column, row)
                   : TableCellStyle();
}

TablePosition SheetCell::position() const {
  return exists_() ? m_adapter2->sheet_cell_position(m_identifier)
                   : TablePosition(0, 0);
}

bool SheetCell::is_covered() const {
  return exists_() && m_adapter2->sheet_cell_is_covered(m_identifier);
}

TableDimensions SheetCell::span() const {
  return exists_() ? m_adapter2->sheet_cell_span(m_identifier)
                   : TableDimensions(1, 1);
}

ValueType SheetCell::value_type() const {
  return exists_() ? m_adapter2->sheet_cell_value_type(m_identifier)
                   : ValueType::unknown;
}

std::string Page::name() const {
  return exists_() ? m_adapter2->page_name(m_identifier) : "";
}

PageLayout Page::page_layout() const {
  return exists_() ? m_adapter2->page_layout(m_identifier) : PageLayout();
}

MasterPage Page::master_page() const {
  if (!exists_()) {
    return {};
  }
  const ElementIdentifier master_page_id =
      m_adapter2->page_master_page(m_identifier);
  return {m_adapter, master_page_id,
          m_adapter->master_page_adapter(master_page_id)};
}

PageLayout MasterPage::page_layout() const {
  return exists_() ? m_adapter2->master_page_page_layout(m_identifier)
                   : PageLayout();
}

TextStyle LineBreak::style() const {
  return exists_() ? m_adapter2->line_break_style(m_identifier) : TextStyle();
}

ParagraphStyle Paragraph::style() const {
  return exists_() ? m_adapter2->paragraph_style(m_identifier)
                   : ParagraphStyle();
}

TextStyle Paragraph::text_style() const {
  return exists_() ? m_adapter2->paragraph_text_style(m_identifier)
                   : TextStyle();
}

TextStyle Span::style() const {
  return exists_() ? m_adapter2->span_style(m_identifier) : TextStyle();
}

std::string Text::content() const {
  return exists_() ? m_adapter2->text_content(m_identifier) : "";
}

void Text::set_content(const std::string &text) const {
  if (!exists_()) {
    return;
  }
  m_adapter2->text_set_content(m_identifier, text);
}

TextStyle Text::style() const {
  return exists_() ? m_adapter2->text_style(m_identifier) : TextStyle();
}

std::string Link::href() const {
  return exists_() ? m_adapter2->link_href(m_identifier) : "";
}

std::string Bookmark::name() const {
  return exists_() ? m_adapter2->bookmark_name(m_identifier) : "";
}

ListType List::type() const {
  return exists_() ? m_adapter2->list_type(m_identifier) : ListType::unordered;
}

TextStyle ListItem::style() const {
  return exists_() ? m_adapter2->list_item_style(m_identifier) : TextStyle();
}

std::string ListItem::marker() const {
  return exists_() ? m_adapter2->list_item_marker(m_identifier) : "";
}

std::optional<std::uint32_t> ListItem::number() const {
  return exists_() ? m_adapter2->list_item_number(m_identifier) : std::nullopt;
}

TableRow Table::first_row() const {
  if (!exists_()) {
    return {};
  }
  const ElementIdentifier row_id = m_adapter2->table_first_row(m_identifier);
  return {m_adapter, row_id, m_adapter->table_row_adapter(row_id)};
}

TableColumn Table::first_column() const {
  if (!exists_()) {
    return {};
  }
  const ElementIdentifier column_id =
      m_adapter2->table_first_column(m_identifier);
  return {m_adapter, column_id, m_adapter->table_column_adapter(column_id)};
}

ElementRange Table::columns() const {
  return exists_()
             ? ElementRange(ElementIterator(
                   m_adapter, m_adapter2->table_first_column(m_identifier)))
             : ElementRange();
}

ElementRange Table::rows() const {
  return exists_() ? ElementRange(ElementIterator(
                         m_adapter, m_adapter2->table_first_row(m_identifier)))
                   : ElementRange();
}

TableDimensions Table::dimensions() const {
  return exists_() ? m_adapter2->table_dimensions(m_identifier)
                   : TableDimensions();
}

TableStyle Table::style() const {
  return exists_() ? m_adapter2->table_style(m_identifier) : TableStyle();
}

TableColumnStyle TableColumn::style() const {
  return exists_() ? m_adapter2->table_column_style(m_identifier)
                   : TableColumnStyle();
}

TableRowStyle TableRow::style() const {
  return exists_() ? m_adapter2->table_row_style(m_identifier)
                   : TableRowStyle();
}

bool TableCell::is_covered() const {
  return exists_() && m_adapter2->table_cell_is_covered(m_identifier);
}

TableDimensions TableCell::span() const {
  return exists_() ? m_adapter2->table_cell_span(m_identifier)
                   : TableDimensions();
}

ValueType TableCell::value_type() const {
  return exists_() ? m_adapter2->table_cell_value_type(m_identifier)
                   : ValueType::string;
}

TableCellStyle TableCell::style() const {
  return exists_() ? m_adapter2->table_cell_style(m_identifier)
                   : TableCellStyle();
}

AnchorType Frame::anchor_type() const {
  return exists_() ? m_adapter2->frame_anchor_type(m_identifier)
                   : AnchorType::as_char; // TODO default?
}

std::optional<Measure> Frame::x() const {
  return exists_() ? m_adapter2->frame_x(m_identifier)
                   : std::optional<Measure>();
}

std::optional<Measure> Frame::y() const {
  return exists_() ? m_adapter2->frame_y(m_identifier)
                   : std::optional<Measure>();
}

std::optional<Measure> Frame::width() const {
  return exists_() ? m_adapter2->frame_width(m_identifier)
                   : std::optional<Measure>();
}

std::optional<Measure> Frame::height() const {
  return exists_() ? m_adapter2->frame_height(m_identifier)
                   : std::optional<Measure>();
}

std::optional<std::int32_t> Frame::z_index() const {
  return exists_() ? m_adapter2->frame_z_index(m_identifier)
                   : std::optional<std::int32_t>();
}

std::optional<DrawingTransform> Frame::transform() const {
  return exists_() ? m_adapter2->frame_transform(m_identifier)
                   : std::optional<DrawingTransform>();
}

GraphicStyle Frame::style() const {
  return exists_() ? m_adapter2->frame_style(m_identifier) : GraphicStyle();
}

Measure Rect::x() const {
  return exists_() ? m_adapter2->rect_x(m_identifier) : Measure(0, {});
}

Measure Rect::y() const {
  return exists_() ? m_adapter2->rect_y(m_identifier) : Measure(0, {});
}

Measure Rect::width() const {
  return exists_() ? m_adapter2->rect_width(m_identifier) : Measure(0, {});
}

Measure Rect::height() const {
  return exists_() ? m_adapter2->rect_height(m_identifier) : Measure(0, {});
}

std::optional<DrawingTransform> Rect::transform() const {
  return exists_() ? m_adapter2->rect_transform(m_identifier)
                   : std::optional<DrawingTransform>();
}

GraphicStyle Rect::style() const {
  return exists_() ? m_adapter2->rect_style(m_identifier) : GraphicStyle();
}

Measure Line::x1() const {
  return exists_() ? m_adapter2->line_x1(m_identifier) : Measure(0, {});
}

Measure Line::y1() const {
  return exists_() ? m_adapter2->line_y1(m_identifier) : Measure(0, {});
}

Measure Line::x2() const {
  return exists_() ? m_adapter2->line_x2(m_identifier) : Measure(0, {});
}

Measure Line::y2() const {
  return exists_() ? m_adapter2->line_y2(m_identifier) : Measure(0, {});
}

std::optional<DrawingTransform> Line::transform() const {
  return exists_() ? m_adapter2->line_transform(m_identifier)
                   : std::optional<DrawingTransform>();
}

GraphicStyle Line::style() const {
  return exists_() ? m_adapter2->line_style(m_identifier) : GraphicStyle();
}

Measure Circle::x() const {
  return exists_() ? m_adapter2->circle_x(m_identifier) : Measure(0, {});
}

Measure Circle::y() const {
  return exists_() ? m_adapter2->circle_y(m_identifier) : Measure(0, {});
}

Measure Circle::width() const {
  return exists_() ? m_adapter2->circle_width(m_identifier) : Measure(0, {});
}

Measure Circle::height() const {
  return exists_() ? m_adapter2->circle_height(m_identifier) : Measure(0, {});
}

std::optional<DrawingTransform> Circle::transform() const {
  return exists_() ? m_adapter2->circle_transform(m_identifier)
                   : std::optional<DrawingTransform>();
}

GraphicStyle Circle::style() const {
  return exists_() ? m_adapter2->circle_style(m_identifier) : GraphicStyle();
}

std::optional<Measure> CustomShape::x() const {
  return exists_() ? m_adapter2->custom_shape_x(m_identifier)
                   : std::optional<Measure>();
}

std::optional<Measure> CustomShape::y() const {
  return exists_() ? m_adapter2->custom_shape_y(m_identifier)
                   : std::optional<Measure>();
}

Measure CustomShape::width() const {
  return exists_() ? m_adapter2->custom_shape_width(m_identifier)
                   : Measure(0, {});
}

Measure CustomShape::height() const {
  return exists_() ? m_adapter2->custom_shape_height(m_identifier)
                   : Measure(0, {});
}

std::optional<DrawingTransform> CustomShape::transform() const {
  return exists_() ? m_adapter2->custom_shape_transform(m_identifier)
                   : std::optional<DrawingTransform>();
}

std::optional<DrawingPath> CustomShape::path() const {
  return exists_() ? m_adapter2->custom_shape_path(m_identifier)
                   : std::optional<DrawingPath>();
}

GraphicStyle CustomShape::style() const {
  return exists_() ? m_adapter2->custom_shape_style(m_identifier)
                   : GraphicStyle();
}

bool Image::is_internal() const {
  return exists_() && m_adapter2->image_is_internal(m_identifier);
}

std::optional<File> Image::file() const {
  return exists_() ? m_adapter2->image_file(m_identifier)
                   : std::optional<File>();
}

std::string Image::href() const {
  return exists_() ? m_adapter2->image_href(m_identifier) : "";
}

} // namespace odr
