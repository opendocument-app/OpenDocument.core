#include <odr/internal/common/document_element.hpp>

#include <odr/document_element_identifier.hpp>

namespace odr::internal {

const abstract::TextRootAdapter *
ElementAdapter::text_root_adapter(ExtendedElementIdentifier) const {
  return m_text_root_adapter;
}

const abstract::SlideAdapter *
ElementAdapter::slide_adapter(ExtendedElementIdentifier) const {
  return m_slide_adapter;
}

const abstract::PageAdapter *
ElementAdapter::page_adapter(ExtendedElementIdentifier) const {
  return m_page_adapter;
}

const abstract::SheetAdapter *
ElementAdapter::sheet_adapter(ExtendedElementIdentifier) const {
  return m_sheet_adapter;
}

const abstract::SheetColumnAdapter *
ElementAdapter::sheet_column_adapter(ExtendedElementIdentifier) const {
  return m_sheet_column_adapter;
}

const abstract::SheetRowAdapter *
ElementAdapter::sheet_row_adapter(ExtendedElementIdentifier) const {
  return m_sheet_row_adapter;
}

const abstract::SheetCellAdapter *
ElementAdapter::sheet_cell_adapter(ExtendedElementIdentifier) const {
  return m_sheet_cell_adapter;
}

const abstract::MasterPageAdapter *
ElementAdapter::master_page_adapter(ExtendedElementIdentifier) const {
  return m_master_page_adapter;
}

const abstract::LineBreakAdapter *
ElementAdapter::line_break_adapter(ExtendedElementIdentifier) const {
  return m_line_break_adapter;
}

const abstract::ParagraphAdapter *
ElementAdapter::paragraph_adapter(ExtendedElementIdentifier) const {
  return m_paragraph_adapter;
}

const abstract::SpanAdapter *
ElementAdapter::span_adapter(ExtendedElementIdentifier) const {
  return m_span_adapter;
}

const abstract::TextAdapter *
ElementAdapter::text_adapter(ExtendedElementIdentifier) const {
  return m_text_adapter;
}

const abstract::LinkAdapter *
ElementAdapter::link_adapter(ExtendedElementIdentifier) const {
  return m_link_adapter;
}

const abstract::BookmarkAdapter *
ElementAdapter::bookmark_adapter(ExtendedElementIdentifier) const {
  return m_bookmark_adapter;
}

const abstract::ListItemAdapter *
ElementAdapter::list_item_adapter(ExtendedElementIdentifier) const {
  return m_list_item_adapter;
}

const abstract::TableAdapter *
ElementAdapter::table_adapter(ExtendedElementIdentifier) const {
  return m_table_adapter;
}

const abstract::TableColumnAdapter *
ElementAdapter::table_column_adapter(ExtendedElementIdentifier) const {
  return m_table_column_adapter;
}

const abstract::TableRowAdapter *
ElementAdapter::table_row_adapter(ExtendedElementIdentifier) const {
  return m_table_row_adapter;
}

const abstract::TableCellAdapter *
ElementAdapter::table_cell_adapter(ExtendedElementIdentifier) const {
  return m_table_cell_adapter;
}

const abstract::FrameAdapter *
ElementAdapter::frame_adapter(ExtendedElementIdentifier) const {
  return m_frame_adapter;
}

const abstract::RectAdapter *
ElementAdapter::rect_adapter(ExtendedElementIdentifier) const {
  return m_rect_adapter;
}

const abstract::LineAdapter *
ElementAdapter::line_adapter(ExtendedElementIdentifier) const {
  return m_line_adapter;
}

const abstract::CircleAdapter *
ElementAdapter::circle_adapter(ExtendedElementIdentifier) const {
  return m_circle_adapter;
}

const abstract::CustomShapeAdapter *
ElementAdapter::custom_shape_adapter(ExtendedElementIdentifier) const {
  return m_custom_shape_adapter;
}

const abstract::ImageAdapter *
ElementAdapter::image_adapter(ExtendedElementIdentifier) const {
  return m_image_adapter;
}

} // namespace odr::internal
