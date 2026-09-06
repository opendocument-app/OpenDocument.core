#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/util/document_util.hpp>

#include <type_traits>

namespace odr::internal {

/// Answers the `*_adapter(id)` hooks for the @p Adapters it is given, and
/// inherits them: a hook whose adapter is in the pack returns `this` for its
/// element type, the rest keep the abstract nullptr.
template <typename... Adapters>
class ElementAdapter : public abstract::ElementAdapter, public Adapters... {
public:
  [[nodiscard]] bool element_is_unique(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool element_is_self_locatable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool element_is_editable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return false;
  }
  [[nodiscard]] DocumentPath
  element_document_path(const ElementIdentifier element_id) const override {
    return util::document::extract_path(*this, element_id, null_element_id);
  }
  [[nodiscard]] ElementIdentifier
  element_navigate_path(const ElementIdentifier element_id,
                        const DocumentPath &path) const override {
    return util::document::navigate_path(*this, element_id, path);
  }

  [[nodiscard]] const abstract::TextRootAdapter *
  text_root_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::TextRootAdapter, ElementType::root>(element_id);
  }
  [[nodiscard]] const abstract::SlideAdapter *
  slide_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::SlideAdapter, ElementType::slide>(element_id);
  }
  [[nodiscard]] const abstract::PageAdapter *
  page_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::PageAdapter, ElementType::page>(element_id);
  }
  [[nodiscard]] const abstract::SheetAdapter *
  sheet_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::SheetAdapter, ElementType::sheet>(element_id);
  }
  [[nodiscard]] const abstract::SheetCellAdapter *
  sheet_cell_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::SheetCellAdapter, ElementType::sheet_cell>(
        element_id);
  }
  [[nodiscard]] const abstract::MasterPageAdapter *
  master_page_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::MasterPageAdapter, ElementType::master_page>(
        element_id);
  }
  [[nodiscard]] const abstract::LineBreakAdapter *
  line_break_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::LineBreakAdapter, ElementType::line_break>(
        element_id);
  }
  [[nodiscard]] const abstract::ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::ParagraphAdapter, ElementType::paragraph>(
        element_id);
  }
  [[nodiscard]] const abstract::SpanAdapter *
  span_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::SpanAdapter, ElementType::span>(element_id);
  }
  [[nodiscard]] const abstract::TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::TextAdapter, ElementType::text>(element_id);
  }
  [[nodiscard]] const abstract::LinkAdapter *
  link_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::LinkAdapter, ElementType::link>(element_id);
  }
  [[nodiscard]] const abstract::BookmarkAdapter *
  bookmark_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::BookmarkAdapter, ElementType::bookmark>(
        element_id);
  }
  [[nodiscard]] const abstract::ListAdapter *
  list_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::ListAdapter, ElementType::list>(element_id);
  }
  [[nodiscard]] const abstract::ListItemAdapter *
  list_item_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::ListItemAdapter, ElementType::list_item>(
        element_id);
  }
  [[nodiscard]] const abstract::TableAdapter *
  table_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::TableAdapter, ElementType::table>(element_id);
  }
  [[nodiscard]] const abstract::TableColumnAdapter *
  table_column_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::TableColumnAdapter, ElementType::table_column>(
        element_id);
  }
  [[nodiscard]] const abstract::TableRowAdapter *
  table_row_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::TableRowAdapter, ElementType::table_row>(
        element_id);
  }
  [[nodiscard]] const abstract::TableCellAdapter *
  table_cell_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::TableCellAdapter, ElementType::table_cell>(
        element_id);
  }
  [[nodiscard]] const abstract::FrameAdapter *
  frame_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::FrameAdapter, ElementType::frame>(element_id);
  }
  [[nodiscard]] const abstract::RectAdapter *
  rect_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::RectAdapter, ElementType::rect>(element_id);
  }
  [[nodiscard]] const abstract::LineAdapter *
  line_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::LineAdapter, ElementType::line>(element_id);
  }
  [[nodiscard]] const abstract::CircleAdapter *
  circle_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::CircleAdapter, ElementType::circle>(element_id);
  }
  [[nodiscard]] const abstract::CustomShapeAdapter *
  custom_shape_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::CustomShapeAdapter, ElementType::custom_shape>(
        element_id);
  }
  [[nodiscard]] const abstract::ImageAdapter *
  image_adapter(const ElementIdentifier element_id) const override {
    return adapter_<abstract::ImageAdapter, ElementType::image>(element_id);
  }

private:
  template <typename Adapter, ElementType type>
  [[nodiscard]] const Adapter *
  adapter_(const ElementIdentifier element_id) const {
    if constexpr ((std::is_same_v<Adapter, Adapters> || ...)) {
      return element_type(element_id) == type ? this : nullptr;
    } else {
      return nullptr;
    }
  }
};

/// Navigates the tree of a registry whose `element_at(id)` yields the links.
template <typename Registry, typename... Adapters>
class RegistryElementAdapter : public ElementAdapter<Adapters...> {
public:
  explicit RegistryElementAdapter(Registry &registry) : m_registry(&registry) {}

  [[nodiscard]] ElementType
  element_type(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).type;
  }

  [[nodiscard]] ElementIdentifier
  element_parent(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).parent_id;
  }
  [[nodiscard]] ElementIdentifier
  element_first_child(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).first_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_last_child(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).last_child_id;
  }
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).previous_sibling_id;
  }
  [[nodiscard]] ElementIdentifier
  element_next_sibling(const ElementIdentifier element_id) const override {
    return m_registry->element_at(element_id).next_sibling_id;
  }

protected:
  Registry *m_registry{nullptr};
};

} // namespace odr::internal
