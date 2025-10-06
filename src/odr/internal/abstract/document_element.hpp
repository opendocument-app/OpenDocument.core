#pragma once

#include <odr/document_element.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace odr {
enum class ElementType;
class ExtendedElementIdentifier;
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
  element_type(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ExtendedElementIdentifier
  element_parent(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ExtendedElementIdentifier
  element_first_child(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ExtendedElementIdentifier
  element_last_child(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ExtendedElementIdentifier
  element_previous_sibling(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ExtendedElementIdentifier
  element_next_sibling(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual bool
  element_is_editable(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual const TextRootAdapter *
  text_root_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SlideAdapter *
  slide_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const PageAdapter *
  page_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SheetAdapter *
  sheet_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SheetColumnAdapter *
  sheet_column_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SheetRowAdapter *
  sheet_row_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SheetCellAdapter *
  sheet_cell_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const MasterPageAdapter *
  master_page_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const LineBreakAdapter *
  line_break_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const ParagraphAdapter *
  paragraph_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const SpanAdapter *
  span_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TextAdapter *
  text_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const LinkAdapter *
  link_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const BookmarkAdapter *
  bookmark_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const ListItemAdapter *
  list_item_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableAdapter *
  table_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableColumnAdapter *
  table_column_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableRowAdapter *
  table_row_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const TableCellAdapter *
  table_cell_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const FrameAdapter *
  frame_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const RectAdapter *
  rect_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const LineAdapter *
  line_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const CircleAdapter *
  circle_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const CustomShapeAdapter *
  custom_shape_adapter(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual const ImageAdapter *
  image_adapter(ExtendedElementIdentifier element_id) const = 0;
};

class TextRootAdapter {
public:
  virtual ~TextRootAdapter() = default;

  [[nodiscard]] virtual PageLayout
  text_root_page_layout(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ExtendedElementIdentifier
  text_root_first_master_page(ExtendedElementIdentifier element_id) const = 0;
};

class SlideAdapter {
public:
  virtual ~SlideAdapter() = default;

  [[nodiscard]] virtual PageLayout
  slide_page_layout(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ExtendedElementIdentifier
  slide_master_page(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual std::string
  slide_name(ExtendedElementIdentifier element_id) const = 0;
};

class PageAdapter {
public:
  virtual ~PageAdapter() = default;

  [[nodiscard]] virtual PageLayout
  page_layout(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ExtendedElementIdentifier
  page_master_page(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual std::string
  page_name(ExtendedElementIdentifier element_id) const = 0;
};

class SheetAdapter {
public:
  virtual ~SheetAdapter() = default;

  [[nodiscard]] virtual std::string
  sheet_name(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableDimensions
  sheet_dimensions(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TableDimensions
  sheet_content(ExtendedElementIdentifier element_id,
                std::optional<TableDimensions> range) const = 0;

  [[nodiscard]] virtual ExtendedElementIdentifier
  sheet_column(ExtendedElementIdentifier element_id,
               std::uint32_t column) const = 0;
  [[nodiscard]] virtual ExtendedElementIdentifier
  sheet_row(ExtendedElementIdentifier element_id, std::uint32_t row) const = 0;
  [[nodiscard]] virtual ExtendedElementIdentifier
  sheet_cell(ExtendedElementIdentifier element_id, std::uint32_t column,
             std::uint32_t row) const = 0;

  [[nodiscard]] virtual ExtendedElementIdentifier
  sheet_first_shape(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableStyle
  sheet_style(ExtendedElementIdentifier element_id) const = 0;
};

class SheetColumnAdapter {
public:
  virtual ~SheetColumnAdapter() = default;

  [[nodiscard]] virtual TableColumnStyle
  sheet_column_style(ExtendedElementIdentifier element_id) const = 0;
};

class SheetRowAdapter {
public:
  virtual ~SheetRowAdapter() = default;

  [[nodiscard]] virtual TableRowStyle
  sheet_row_style(ExtendedElementIdentifier element_id) const = 0;
};

class SheetCellAdapter {
public:
  virtual ~SheetCellAdapter() = default;

  [[nodiscard]] virtual std::uint32_t
  sheet_cell_column(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::uint32_t
  sheet_cell_row(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual bool
  sheet_cell_is_covered(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TableDimensions
  sheet_cell_span(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ValueType
  sheet_cell_value_type(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableCellStyle
  sheet_cell_style(ExtendedElementIdentifier element_id) const = 0;
};

class MasterPageAdapter {
public:
  virtual ~MasterPageAdapter() = default;

  [[nodiscard]] virtual PageLayout
  master_page_page_layout(ExtendedElementIdentifier element_id) const = 0;
};

class LineBreakAdapter {
public:
  virtual ~LineBreakAdapter() = default;

  [[nodiscard]] virtual TextStyle
  line_break_style(ExtendedElementIdentifier element_id) const = 0;
};

class ParagraphAdapter {
public:
  virtual ~ParagraphAdapter() = default;

  [[nodiscard]] virtual ParagraphStyle
  paragraph_style(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TextStyle
  paragraph_text_style(ExtendedElementIdentifier element_id) const = 0;
};

class SpanAdapter {
public:
  virtual ~SpanAdapter() = default;

  [[nodiscard]] virtual TextStyle
  span_style(ExtendedElementIdentifier element_id) const = 0;
};

class TextAdapter {
public:
  virtual ~TextAdapter() = default;

  [[nodiscard]] virtual std::string
  text_content(ExtendedElementIdentifier element_id) const = 0;
  virtual void text_set_content(ExtendedElementIdentifier element_id,
                                const std::string &text) const = 0;

  [[nodiscard]] virtual TextStyle
  text_style(ExtendedElementIdentifier element_id) const = 0;
};

class LinkAdapter {
public:
  virtual ~LinkAdapter() = default;

  [[nodiscard]] virtual std::string
  link_href(ExtendedElementIdentifier element_id) const = 0;
};

class BookmarkAdapter {
public:
  virtual ~BookmarkAdapter() = default;

  [[nodiscard]] virtual std::string
  bookmark_name(ExtendedElementIdentifier element_id) const = 0;
};

class ListItemAdapter {
public:
  virtual ~ListItemAdapter() = default;

  [[nodiscard]] virtual TextStyle
  list_item_style(ExtendedElementIdentifier element_id) const = 0;
};

class TableAdapter {
public:
  virtual ~TableAdapter() = default;

  [[nodiscard]] virtual TableDimensions
  table_dimensions(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual ExtendedElementIdentifier
  table_first_column(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ExtendedElementIdentifier
  table_first_row(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableStyle
  table_style(ExtendedElementIdentifier element_id) const = 0;
};

class TableColumnAdapter {
public:
  virtual ~TableColumnAdapter() = default;

  [[nodiscard]] virtual TableColumnStyle
  table_column_style(ExtendedElementIdentifier element_id) const = 0;
};

class TableRowAdapter {
public:
  virtual ~TableRowAdapter() = default;

  [[nodiscard]] virtual TableRowStyle
  table_row_style(ExtendedElementIdentifier element_id) const = 0;
};

class TableCellAdapter {
public:
  virtual ~TableCellAdapter() = default;

  [[nodiscard]] virtual bool
  table_cell_is_covered(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual TableDimensions
  table_cell_span(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual ValueType
  table_cell_value_type(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual TableCellStyle
  table_cell_style(ExtendedElementIdentifier element_id) const = 0;
};

class FrameAdapter {
public:
  virtual ~FrameAdapter() = default;

  [[nodiscard]] virtual AnchorType
  frame_anchor_type(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_x(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_y(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_width(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_height(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  frame_z_index(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  frame_style(ExtendedElementIdentifier element_id) const = 0;
};

class RectAdapter {
public:
  virtual ~RectAdapter() = default;

  [[nodiscard]] virtual std::string
  rect_x(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  rect_y(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  rect_width(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  rect_height(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  rect_style(ExtendedElementIdentifier element_id) const = 0;
};

class LineAdapter {
public:
  virtual ~LineAdapter() = default;

  [[nodiscard]] virtual std::string
  line_x1(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  line_y1(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  line_x2(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  line_y2(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  line_style(ExtendedElementIdentifier element_id) const = 0;
};

class CircleAdapter {
public:
  virtual ~CircleAdapter() = default;

  [[nodiscard]] virtual std::string
  circle_x(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  circle_y(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  circle_width(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  circle_height(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  circle_style(ExtendedElementIdentifier element_id) const = 0;
};

class CustomShapeAdapter {
public:
  virtual ~CustomShapeAdapter() = default;

  [[nodiscard]] virtual std::optional<std::string>
  custom_shape_x(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<std::string>
  custom_shape_y(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  custom_shape_width(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  custom_shape_height(ExtendedElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual GraphicStyle
  custom_shape_style(ExtendedElementIdentifier element_id) const = 0;
};

class ImageAdapter {
public:
  virtual ~ImageAdapter() = default;

  [[nodiscard]] virtual bool
  image_is_internal(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::optional<odr::File>
  image_file(ExtendedElementIdentifier element_id) const = 0;
  [[nodiscard]] virtual std::string
  image_href(ExtendedElementIdentifier element_id) const = 0;
};

} // namespace odr::internal::abstract
