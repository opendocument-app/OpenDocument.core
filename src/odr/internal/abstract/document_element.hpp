#pragma once

#include <odr/document_element.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace odr {
enum class ElementType;
class File;
} // namespace odr

namespace odr::internal::abstract {
class TextRootAdapter;
class SlideAdapter;
class PageAdapter;
class SheetAdapter;
class SheetColumnAdapter;
class SheetRowAdapter;
class SheetCellAdapter;
class MasterPageAdapter;
class LineBreakAdapter;
class ParagraphAdapter;
class SpanAdapter;
class TextAdapter;
class LinkAdapter;
class BookmarkAdapter;
class ListItemAdapter;
class TableAdapter;
class TableColumnAdapter;
class TableRowAdapter;
class TableCellAdapter;
class FrameAdapter;
class RectAdapter;
class LineAdapter;
class CircleAdapter;
class CustomShapeAdapter;
class ImageAdapter;

class ElementAdapter {
public:
  virtual ~ElementAdapter() = default;

  [[nodiscard]] virtual ElementType
  element_type(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ElementIdentifier
  element_parent(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  element_first_child(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  element_last_child(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  element_previous_sibling(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  element_next_sibling(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual bool
  element_is_editable(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual const TextRootAdapter *
  text_root_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SlideAdapter *
  slide_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const PageAdapter *
  page_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SheetAdapter *
  sheet_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SheetCellAdapter *
  sheet_cell_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const MasterPageAdapter *
  master_page_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const LineBreakAdapter *
  line_break_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const ParagraphAdapter *
  paragraph_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SpanAdapter *
  span_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TextAdapter *
  text_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const LinkAdapter *
  link_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const BookmarkAdapter *
  bookmark_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const ListItemAdapter *
  list_item_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableAdapter *
  table_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableColumnAdapter *
  table_column_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableRowAdapter *
  table_row_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableCellAdapter *
  table_cell_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const FrameAdapter *
  frame_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const RectAdapter *
  rect_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const LineAdapter *
  line_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const CircleAdapter *
  circle_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const CustomShapeAdapter *
  custom_shape_adapter(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const ImageAdapter *
  image_adapter(ElementIdentifier element_id) const = 0;
};

class TextRootAdapter {
public:
  virtual ~TextRootAdapter() = default;

  [[nodiscard]] virtual PageLayout
  text_root_page_layout(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ElementIdentifier
  text_root_first_master_page(ElementIdentifier element_id) const = 0;
};

class SlideAdapter {
public:
  virtual ~SlideAdapter() = default;

  [[nodiscard]] virtual PageLayout
  slide_page_layout(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ElementIdentifier
  slide_master_page(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual std::string
  slide_name(ElementIdentifier element_id) const = 0;
};

class PageAdapter {
public:
  virtual ~PageAdapter() = default;

  [[nodiscard]] virtual PageLayout
  page_layout(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ElementIdentifier
  page_master_page(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual std::string
  page_name(ElementIdentifier element_id) const = 0;
};

class SheetAdapter {
public:
  virtual ~SheetAdapter() = default;

  [[nodiscard]] virtual std::string
  sheet_name(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableDimensions
  sheet_dimensions(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TableDimensions
  sheet_content(ElementIdentifier element_id,
                std::optional<TableDimensions> range) const = 0;

  [[nodiscard]] virtual ElementIdentifier
  sheet_cell(ElementIdentifier element_id, std::uint32_t column,
             std::uint32_t row) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  sheet_first_shape(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableStyle
  sheet_style(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TableColumnStyle
  sheet_column_style(ElementIdentifier element_id,
                     std::uint32_t column) const = 0;
  [[nodiscard]] virtual TableRowStyle
  sheet_row_style(ElementIdentifier element_id, std::uint32_t row) const = 0;
};

class SheetCellAdapter {
public:
  virtual ~SheetCellAdapter() = default;

  [[nodiscard]] virtual TablePosition
  sheet_cell_position(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual bool
  sheet_cell_is_covered(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TableDimensions
  sheet_cell_span(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ValueType
  sheet_cell_value_type(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableCellStyle
  sheet_cell_style(ElementIdentifier element_id) const = 0;
};

class MasterPageAdapter {
public:
  virtual ~MasterPageAdapter() = default;

  [[nodiscard]] virtual PageLayout
  master_page_page_layout(ElementIdentifier element_id) const = 0;
};

class LineBreakAdapter {
public:
  virtual ~LineBreakAdapter() = default;

  [[nodiscard]] virtual TextStyle
  line_break_style(ElementIdentifier element_id) const = 0;
};

class ParagraphAdapter {
public:
  virtual ~ParagraphAdapter() = default;

  [[nodiscard]] virtual ParagraphStyle
  paragraph_style(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TextStyle
  paragraph_text_style(ElementIdentifier element_id) const = 0;
};

class SpanAdapter {
public:
  virtual ~SpanAdapter() = default;

  [[nodiscard]] virtual TextStyle
  span_style(ElementIdentifier element_id) const = 0;
};

class TextAdapter {
public:
  virtual ~TextAdapter() = default;

  [[nodiscard]] virtual std::string
  text_content(ElementIdentifier element_id) const = 0;
  virtual void text_set_content(ElementIdentifier element_id,
                                const std::string &text) const = 0;

  [[nodiscard]] virtual TextStyle
  text_style(ElementIdentifier element_id) const = 0;
};

class LinkAdapter {
public:
  virtual ~LinkAdapter() = default;

  [[nodiscard]] virtual std::string
  link_href(ElementIdentifier element_id) const = 0;
};

class BookmarkAdapter {
public:
  virtual ~BookmarkAdapter() = default;

  [[nodiscard]] virtual std::string
  bookmark_name(ElementIdentifier element_id) const = 0;
};

class ListItemAdapter {
public:
  virtual ~ListItemAdapter() = default;

  [[nodiscard]] virtual TextStyle
  list_item_style(ElementIdentifier element_id) const = 0;
};

class TableAdapter {
public:
  virtual ~TableAdapter() = default;

  [[nodiscard]] virtual TableDimensions
  table_dimensions(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ElementIdentifier
  table_first_column(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ElementIdentifier
  table_first_row(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableStyle
  table_style(ElementIdentifier element_id) const = 0;
};

class TableColumnAdapter {
public:
  virtual ~TableColumnAdapter() = default;

  [[nodiscard]] virtual TableColumnStyle
  table_column_style(ElementIdentifier element_id) const = 0;
};

class TableRowAdapter {
public:
  virtual ~TableRowAdapter() = default;

  [[nodiscard]] virtual TableRowStyle
  table_row_style(ElementIdentifier element_id) const = 0;
};

class TableCellAdapter {
public:
  virtual ~TableCellAdapter() = default;

  [[nodiscard]] virtual bool
  table_cell_is_covered(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TableDimensions
  table_cell_span(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ValueType
  table_cell_value_type(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableCellStyle
  table_cell_style(ElementIdentifier element_id) const = 0;
};

class FrameAdapter {
public:
  virtual ~FrameAdapter() = default;

  [[nodiscard]] virtual AnchorType
  frame_anchor_type(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_x(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_y(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_width(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_height(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_z_index(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  frame_style(ElementIdentifier element_id) const = 0;
};

class RectAdapter {
public:
  virtual ~RectAdapter() = default;

  [[nodiscard]] virtual std::string
  rect_x(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  rect_y(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  rect_width(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  rect_height(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  rect_style(ElementIdentifier element_id) const = 0;
};

class LineAdapter {
public:
  virtual ~LineAdapter() = default;

  [[nodiscard]] virtual std::string
  line_x1(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  line_y1(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  line_x2(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  line_y2(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  line_style(ElementIdentifier element_id) const = 0;
};

class CircleAdapter {
public:
  virtual ~CircleAdapter() = default;

  [[nodiscard]] virtual std::string
  circle_x(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  circle_y(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  circle_width(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  circle_height(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  circle_style(ElementIdentifier element_id) const = 0;
};

class CustomShapeAdapter {
public:
  virtual ~CustomShapeAdapter() = default;

  [[nodiscard]] virtual std::optional<std::string>
  custom_shape_x(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  custom_shape_y(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  custom_shape_width(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  custom_shape_height(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  custom_shape_style(ElementIdentifier element_id) const = 0;
};

class ImageAdapter {
public:
  virtual ~ImageAdapter() = default;

  [[nodiscard]] virtual bool
  image_is_internal(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<odr::File>
  image_file(ElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  image_href(ElementIdentifier element_id) const = 0;
};

} // namespace odr::internal::abstract
