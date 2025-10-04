#pragma once

#include <odr/internal/abstract/document_element.hpp>

namespace odr::internal {

class ElementAdapter : public abstract::ElementAdapter {
public:
  [[nodiscard]] const abstract::TextRootAdapter *
  text_root_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::SlideAdapter *
  slide_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::PageAdapter *
  page_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::SheetAdapter *
  sheet_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::SheetColumnAdapter *
  sheet_column_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::SheetRowAdapter *
  sheet_row_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::SheetCellAdapter *
  sheet_cell_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::MasterPageAdapter *
  master_page_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::LineBreakAdapter *
  line_break_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::ParagraphAdapter *
  paragraph_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::SpanAdapter *
  span_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::TextAdapter *
  text_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::LinkAdapter *
  link_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::BookmarkAdapter *
  bookmark_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::ListItemAdapter *
  list_item_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::TableAdapter *
  table_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::TableColumnAdapter *
  table_column_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::TableRowAdapter *
  table_row_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::TableCellAdapter *
  table_cell_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::FrameAdapter *
  frame_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::RectAdapter *
  rect_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::LineAdapter *
  line_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::CircleAdapter *
  circle_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::CustomShapeAdapter *
  custom_shape_adapter(ExtendedElementIdentifier element_id) const override;
  [[nodiscard]] const abstract::ImageAdapter *
  image_adapter(ExtendedElementIdentifier element_id) const override;

protected:
  abstract::TextRootAdapter *m_text_root_adapter{nullptr};
  abstract::SlideAdapter *m_slide_adapter{nullptr};
  abstract::PageAdapter *m_page_adapter{nullptr};
  abstract::SheetAdapter *m_sheet_adapter{nullptr};
  abstract::SheetColumnAdapter *m_sheet_column_adapter{nullptr};
  abstract::SheetRowAdapter *m_sheet_row_adapter{nullptr};
  abstract::SheetCellAdapter *m_sheet_cell_adapter{nullptr};
  abstract::MasterPageAdapter *m_master_page_adapter{nullptr};
  abstract::LineBreakAdapter *m_line_break_adapter{nullptr};
  abstract::ParagraphAdapter *m_paragraph_adapter{nullptr};
  abstract::SpanAdapter *m_span_adapter{nullptr};
  abstract::TextAdapter *m_text_adapter{nullptr};
  abstract::LinkAdapter *m_link_adapter{nullptr};
  abstract::BookmarkAdapter *m_bookmark_adapter{nullptr};
  abstract::ListItemAdapter *m_list_item_adapter{nullptr};
  abstract::TableAdapter *m_table_adapter{nullptr};
  abstract::TableColumnAdapter *m_table_column_adapter{nullptr};
  abstract::TableRowAdapter *m_table_row_adapter{nullptr};
  abstract::TableCellAdapter *m_table_cell_adapter{nullptr};
  abstract::FrameAdapter *m_frame_adapter{nullptr};
  abstract::RectAdapter *m_rect_adapter{nullptr};
  abstract::LineAdapter *m_line_adapter{nullptr};
  abstract::CircleAdapter *m_circle_adapter{nullptr};
  abstract::CustomShapeAdapter *m_custom_shape_adapter{nullptr};
  abstract::ImageAdapter *m_image_adapter{nullptr};
};

} // namespace odr::internal
