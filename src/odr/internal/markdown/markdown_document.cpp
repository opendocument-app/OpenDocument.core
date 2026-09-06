#include <odr/internal/markdown/markdown_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/style.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/markdown/markdown_parser.hpp>

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

namespace {

using AdapterBase = internal::RegistryElementAdapter<
    const ElementRegistry, abstract::TextRootAdapter,
    abstract::LineBreakAdapter, abstract::ParagraphAdapter,
    abstract::SpanAdapter, abstract::TextAdapter, abstract::LinkAdapter,
    abstract::ListAdapter, abstract::ListItemAdapter, abstract::TableAdapter,
    abstract::TableColumnAdapter, abstract::TableRowAdapter,
    abstract::TableCellAdapter>;

class ElementAdapter final : public AdapterBase {
public:
  ElementAdapter(const ElementRegistry &registry,
                 const StyleRegistry &style_registry)
      : AdapterBase(registry), m_style_registry(&style_registry) {}

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
