#include <odr/internal/oldms/spreadsheet/xls_document.hpp>

#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/style.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>
#include <odr/internal/util/document_util.hpp>

#include <algorithm>

namespace odr::internal::oldms::spreadsheet {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry,
                       const StyleRegistry &style_registry);
}

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files)
    : internal::Document(FileType::legacy_excel_worksheets,
                         DocumentType::spreadsheet, std::move(files)) {
  m_root_element = parse_tree(m_element_registry, m_style_registry, *m_files);

  m_element_adapter =
      create_element_adapter(*this, m_element_registry, m_style_registry);
}

ElementRegistry &Document::element_registry() { return m_element_registry; }

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
                             public abstract::SheetAdapter,
                             public abstract::SheetCellAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::TextAdapter {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry,
                 const StyleRegistry &style_registry)
      : m_document(&document), m_registry(&registry),
        m_style_registry(&style_registry) {}

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

  [[nodiscard]] bool element_is_unique(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return true;
  }
  [[nodiscard]] bool element_is_self_locatable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return true;
  }
  [[nodiscard]] bool element_is_editable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return false;
  }
  [[nodiscard]]
  DocumentPath
  element_document_path(const ElementIdentifier element_id) const override {
    return util::document::extract_path(*this, element_id, null_element_id);
  }
  [[nodiscard]] ElementIdentifier
  element_navigate_path(const ElementIdentifier element_id,
                        const DocumentPath &path) const override {
    return util::document::navigate_path(*this, element_id, path);
  }

  [[nodiscard]] const SheetAdapter *
  sheet_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::sheet ? this : nullptr;
  }
  [[nodiscard]] const SheetCellAdapter *
  sheet_cell_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::sheet_cell ? this : nullptr;
  }
  [[nodiscard]] const ParagraphAdapter *
  paragraph_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::paragraph ? this : nullptr;
  }
  [[nodiscard]] const TextAdapter *
  text_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::text ? this : nullptr;
  }

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).name;
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).dimensions;
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    TableDimensions content = m_registry->sheet_element_at(element_id).content;
    if (range.has_value()) {
      content.rows = std::min(content.rows, range->rows);
      content.columns = std::min(content.columns, range->columns);
    }
    return content;
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell(const ElementIdentifier element_id, const std::uint32_t column,
             const std::uint32_t row) const override {
    return m_registry->sheet_element_at(element_id).cell(column, row);
  }
  [[nodiscard]] ElementIdentifier sheet_first_shape(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return null_element_id;
  }
  [[nodiscard]] TableStyle sheet_style(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }
  [[nodiscard]] TableColumnStyle sheet_column_style(
      [[maybe_unused]] const ElementIdentifier element_id,
      [[maybe_unused]] const std::uint32_t column) const override {
    (void)element_id;
    (void)column;
    return {};
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style([[maybe_unused]] const ElementIdentifier element_id,
                  [[maybe_unused]] const std::uint32_t row) const override {
    (void)element_id;
    (void)row;
    return {};
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ElementIdentifier element_id,
                   const std::uint32_t column,
                   const std::uint32_t row) const override {
    const ElementIdentifier cell_id =
        m_registry->sheet_element_at(element_id).cell(column, row);
    if (cell_id == null_element_id) {
      return {};
    }
    const ElementRegistry::SheetCell &cell =
        m_registry->sheet_cell_element_at(cell_id);
    return m_style_registry->cell_style(cell.ixfe).table_cell_style;
  }

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    return m_registry->sheet_cell_element_at(element_id).position;
  }
  [[nodiscard]] bool sheet_cell_is_covered(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return false;
  }
  [[nodiscard]] TableDimensions sheet_cell_span(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return {1, 1};
  }
  [[nodiscard]] ValueType sheet_cell_value_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    (void)element_id;
    return ValueType::string;
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ElementIdentifier element_id) const override {
    (void)element_id;
    return {};
  }
  [[nodiscard]] TextStyle
  paragraph_text_style(const ElementIdentifier element_id) const override {
    return cell_text_style(element_id);
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
    return cell_text_style(element_id);
  }

private:
  // TODO remove maybe_unused
  [[maybe_unused]]
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};
  const StyleRegistry *m_style_registry{nullptr};

  /// The font style of the sheet_cell ancestor (paragraph and text elements
  /// only exist inside cells).
  [[nodiscard]] TextStyle
  cell_text_style(const ElementIdentifier element_id) const {
    ElementIdentifier id = element_id;
    while (id != null_element_id &&
           element_type(id) != ElementType::sheet_cell) {
      id = m_registry->element_at(id).parent_id;
    }
    if (id == null_element_id) {
      return {};
    }
    const ElementRegistry::SheetCell &cell =
        m_registry->sheet_cell_element_at(id);
    return m_style_registry->cell_style(cell.ixfe).text_style;
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry,
                       const StyleRegistry &style_registry) {
  return std::make_unique<ElementAdapter>(document, registry, style_registry);
}

} // namespace

} // namespace odr::internal::oldms::spreadsheet
