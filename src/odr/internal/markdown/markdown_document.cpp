#include <odr/internal/markdown/markdown_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/markdown/markdown_parser.hpp>
#include <odr/internal/util/document_util.hpp>

#include <memory>

namespace odr::internal::markdown {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const ElementRegistry &registry,
                       const StyleRegistry &style_registry);
}

Document::Document(const std::string_view text)
    : internal::Document(FileType::markdown, DocumentType::text, nullptr) {
  m_root_element = parse_tree(m_element_registry, m_style_registry, text);

  m_element_adapter =
      create_element_adapter(m_element_registry, m_style_registry);
}

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

const StyleRegistry &Document::style_registry() const {
  return m_style_registry;
}

bool Document::is_editable() const noexcept { return false; }

bool Document::is_savable(const bool encrypted) const noexcept {
  (void)encrypted;
  return false;
}

void Document::save(const Path &path) const {
  (void)path;
  throw UnsupportedOperation();
}

void Document::save(const Path &path, const char *password) const {
  (void)path;
  (void)password;
  throw UnsupportedOperation();
}

namespace {

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::TextRootAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter,
                             public abstract::LinkAdapter,
                             public abstract::ListAdapter,
                             public abstract::ListItemAdapter,
                             public abstract::TableAdapter,
                             public abstract::TableColumnAdapter,
                             public abstract::TableRowAdapter,
                             public abstract::TableCellAdapter {
public:
  ElementAdapter(const ElementRegistry &registry,
                 const StyleRegistry &style_registry)
      : m_registry(&registry), m_style_registry(&style_registry) {}

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

  [[nodiscard]] bool
  element_is_unique(const ElementIdentifier element_id) const override {
    (void)element_id;
    return true;
  }
  [[nodiscard]] bool
  element_is_self_locatable(const ElementIdentifier element_id) const override {
    (void)element_id;
    return true;
  }
  [[nodiscard]] bool
  element_is_editable(const ElementIdentifier element_id) const override {
    (void)element_id;
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

  [[nodiscard]] const TextRootAdapter *
  text_root_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::root ? this : nullptr;
  }
  [[nodiscard]] const LineBreakAdapter *
  line_break_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::line_break ? this : nullptr;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::paragraph ? this : nullptr;
  }
  [[nodiscard]] const SpanAdapter *
  span_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::span ? this : nullptr;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::text ? this : nullptr;
  }
  [[nodiscard]] const LinkAdapter *
  link_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::link ? this : nullptr;
  }
  [[nodiscard]] const ListAdapter *
  list_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::list ? this : nullptr;
  }
  [[nodiscard]] const ListItemAdapter *
  list_item_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::list_item ? this : nullptr;
  }
  [[nodiscard]] const TableAdapter *
  table_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table ? this : nullptr;
  }
  [[nodiscard]] const TableColumnAdapter *
  table_column_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table_column ? this
                                                                 : nullptr;
  }
  [[nodiscard]] const TableRowAdapter *
  table_row_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table_row ? this : nullptr;
  }
  [[nodiscard]] const TableCellAdapter *
  table_cell_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::table_cell ? this : nullptr;
  }

  /// Markdown is flow content: it has no page, and the viewport is the width.
  [[nodiscard]] PageLayout
  text_root_page_layout(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }
  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      const ElementIdentifier element_id) const override {
    (void)element_id;
    return null_element_id;
  }

  [[nodiscard]] TextStyle
  line_break_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ElementIdentifier element_id) const override {
    return m_style_registry->paragraph_style(
        m_registry->element_paragraph_style_index(element_id));
  }
  [[nodiscard]] TextStyle
  paragraph_text_style(const ElementIdentifier element_id) const override {
    return stored_text_style(element_id);
  }

  [[nodiscard]] TextStyle
  span_style(const ElementIdentifier element_id) const override {
    return stored_text_style(element_id);
  }

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    return m_registry->text_element_at(element_id).text;
  }
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    (void)element_id;
    (void)text;
    throw UnsupportedOperation();
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    // The enclosing span or paragraph carries the character style.
    (void)element_id;
    return {};
  }

  [[nodiscard]] std::string
  link_href(const ElementIdentifier element_id) const override {
    return m_registry->link_element_at(element_id).href;
  }

  [[nodiscard]] ListType
  list_type(const ElementIdentifier element_id) const override {
    return m_registry->list_element_at(element_id).type;
  }

  [[nodiscard]] TextStyle
  list_item_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }
  [[nodiscard]] std::string
  list_item_marker(const ElementIdentifier element_id) const override {
    return m_registry->list_item_element_at(element_id).marker;
  }
  [[nodiscard]] std::optional<std::uint32_t>
  list_item_number(const ElementIdentifier element_id) const override {
    return m_registry->list_item_element_at(element_id).number;
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ElementIdentifier element_id) const override {
    return m_registry->table_element_at(element_id).dimensions;
  }
  [[nodiscard]] ElementIdentifier
  table_first_column(const ElementIdentifier element_id) const override {
    return m_registry->table_element_at(element_id).first_column_id;
  }
  /// The rows are the table's children; the columns hang off a chain of their
  /// own, as they do in odf.
  [[nodiscard]] ElementIdentifier
  table_first_row(const ElementIdentifier element_id) const override {
    return element_first_child(element_id);
  }
  [[nodiscard]] TableStyle
  table_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }

  [[nodiscard]] TableColumnStyle
  table_column_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }

  [[nodiscard]] TableRowStyle
  table_row_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }

  /// A markdown table is a grid of single cells: no merging, no formulas.
  [[nodiscard]] bool
  table_cell_is_covered(const ElementIdentifier element_id) const override {
    (void)element_id;
    return false;
  }
  [[nodiscard]] TableDimensions
  table_cell_span(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {1, 1};
  }
  [[nodiscard]] ValueType
  table_cell_value_type(const ElementIdentifier element_id) const override {
    (void)element_id;
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  table_cell_style(const ElementIdentifier element_id) const override {
    TableCellStyle result;
    result.horizontal_align =
        m_registry->table_cell_element_at(element_id).horizontal_align;
    return result;
  }

private:
  const ElementRegistry *m_registry{nullptr};
  const StyleRegistry *m_style_registry{nullptr};

  [[nodiscard]] TextStyle
  stored_text_style(const ElementIdentifier element_id) const {
    return m_style_registry->text_style(
        m_registry->element_text_style_index(element_id));
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const ElementRegistry &registry,
                       const StyleRegistry &style_registry) {
  return std::make_unique<ElementAdapter>(registry, style_registry);
}

} // namespace

} // namespace odr::internal::markdown
