#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/abstract/table.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/common/table_position.h>
#include <internal/odf/odf_document.h>
#include <internal/util/map_util.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <unordered_map>

namespace odr::internal::odf {

class OpenDocument::Table final : public abstract::Table {
public:
  Table(std::shared_ptr<OpenDocument> document, const pugi::xml_node node)
      : m_document{std::move(document)}, m_node{node} {
    std::uint32_t column_index = 0;
    for (auto column : node.children("table:table-column")) {
      const auto repeat =
          m_node.attribute("table:number-columns-repeated").as_uint(1);

      Column new_column;
      new_column.node = column;
      new_column.index = column_index;
      new_column.repeat = repeat;

      // TODO insert

      column_index += repeat;
    }

    std::uint32_t row_index = 0;
    for (auto row : node.children("table:table-row")) {
      const auto repeat =
          m_node.attribute("table:number-rows-repeated").as_uint(1);

      Row new_row;
      new_row.node = row;
      new_row.index = column_index;
      new_row.repeat = repeat;

      std::uint32_t cell_index = 0;
      for (auto cell : row.children("table:table-cell")) {
        const auto repeat =
            m_node.attribute("table:number-columns-repeated").as_uint(1);

        Cell new_cell;
        new_cell.node = row;
        new_cell.index = cell_index;
        new_cell.repeat = repeat;
        // TODO handle repeated
        // TODO parent?
        new_cell.first_child = m_document->register_children_(cell, {}, {});

        // TODO insert

        cell_index += repeat;
      }

      // TODO insert

      row_index += repeat;
    }
  }

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final {
    return m_document;
  }

  [[nodiscard]] ElementIdentifier
  cell_first_child(const std::uint32_t row,
                   const std::uint32_t column) const final {
    auto c = cell_(row, column);
    if (c == nullptr) {
      return {};
    }
    return c->first_child;
  }

  [[nodiscard]] std::any
  column_property(const std::uint32_t column,
                  const ElementProperty property) const final {
    auto c = column_(column);
    if (c == nullptr) {
      return {};
    }
    return {}; // TODO
  }

  [[nodiscard]] std::any
  row_property(const std::uint32_t row,
               const ElementProperty property) const final {
    auto r = row_(row);
    if (r == nullptr) {
      return {};
    }
    return {}; // TODO
  }

  [[nodiscard]] std::any
  cell_property(const std::uint32_t row, const std::uint32_t column,
                const ElementProperty property) const final {
    auto c = cell_(row, column);
    if (c == nullptr) {
      return {};
    }
    return {}; // TODO
  }

  [[nodiscard]] TableDimensions
  dimensions(const std::uint32_t limit_rows,
             const std::uint32_t limit_cols) const final {
    return {}; // TODO
  }

private:
  struct Column {
    pugi::xml_node node;
    std::uint32_t index{0};
    std::uint32_t repeat{1};
  };

  struct Cell {
    pugi::xml_node node;
    std::uint32_t index{0};
    std::uint32_t repeat{1};
    ElementIdentifier first_child;
  };

  struct Row {
    pugi::xml_node node;
    std::uint32_t index{0};
    std::uint32_t repeat{1};
  };

  std::shared_ptr<OpenDocument> m_document;
  pugi::xml_node m_node;

  std::unordered_map<std::uint32_t, Column> m_columns;
  std::unordered_map<std::uint32_t, Row> m_rows;
  std::unordered_map<common::TablePosition, Cell> m_cells;

  Column *column_(const std::uint32_t column) const {
    return nullptr; // TODO
  }

  Row *row_(const std::uint32_t row) const {
    return nullptr; // TODO
  }

  Cell *cell_(const std::uint32_t row, const std::uint32_t column) const {
    return nullptr; // TODO
  }
};

OpenDocument::OpenDocument(
    const DocumentType document_type,
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_document_type{document_type}, m_filesystem{std::move(filesystem)} {
  m_content_xml = util::xml::parse(*m_filesystem, "content.xml");

  if (m_filesystem->exists("styles.xml")) {
    m_styles_xml = util::xml::parse(*m_filesystem, "styles.xml");
  }

  m_root = register_element_(
      m_content_xml.document_element().child("office:body").first_child(), 0,
      0);

  if (!m_root) {
    throw NoOpenDocumentFile();
  }

  // TODO scan styles

  // TODO
  // m_styles = Styles(m_styles_xml.document_element(),
  // m_content_xml.document_element());
}

ElementIdentifier
OpenDocument::register_element_(const pugi::xml_node node,
                                const ElementIdentifier parent,
                                const ElementIdentifier previous_sibling) {
  static std::unordered_map<std::string, ElementType> element_type_table{
      {"text:p", ElementType::PARAGRAPH},
      {"text:h", ElementType::PARAGRAPH},
      {"text:span", ElementType::SPAN},
      {"text:s", ElementType::TEXT},
      {"text:tab", ElementType::TEXT},
      {"text:line-break", ElementType::LINE_BREAK},
      {"text:a", ElementType::LINK},
      {"text:bookmark", ElementType::BOOKMARK},
      {"text:bookmark-start", ElementType::BOOKMARK},
      {"text:list", ElementType::LIST},
      {"text:list-item", ElementType::LIST_ITEM},
      {"table:table", ElementType::TABLE},
      {"draw:frame", ElementType::FRAME},
      {"draw:image", ElementType::IMAGE},
      {"draw:rect", ElementType::RECT},
      {"draw:line", ElementType::LINE},
      {"draw:circle", ElementType::CIRCLE},
      {"office:text", ElementType::ROOT},
      {"office:presentation", ElementType::ROOT},
      {"office:spreadsheet", ElementType::ROOT},
      {"office:drawing", ElementType::ROOT},
      // TODO "draw:custom-shape"
  };

  if (!node) {
    return 0;
  }

  ElementType element_type = ElementType::NONE;

  if (node.type() == pugi::node_pcdata) {
    element_type = ElementType::TEXT;
  } else if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:table-of-content") {
      return register_children_(node.child("text:index-body"), parent,
                                previous_sibling);
    } else if (element == "draw:g") {
      // drawing group not supported
      return register_children_(node, parent, previous_sibling);
    }

    util::map::lookup_map(element_type_table, element, element_type);
  }

  if (element_type == ElementType::NONE) {
    // TODO log node
    return 0;
  }

  auto new_element = new_element_(node, element_type, parent, previous_sibling);

  if (element_type == ElementType::TABLE) {
    register_table_(node);
  } else {
    register_children_(node, new_element, {});
  }

  return new_element;
}

ElementIdentifier
OpenDocument::register_children_(const pugi::xml_node node,
                                 const ElementIdentifier parent,
                                 ElementIdentifier previous_sibling) {
  ElementIdentifier first_child;

  for (auto &&child_node : node) {
    const ElementIdentifier child =
        register_element_(child_node, parent, previous_sibling);
    if (!child) {
      continue;
    }
    if (!first_child) {
      first_child = child;
    }
    previous_sibling = child;
  }

  return first_child;
}

void OpenDocument::register_table_(const pugi::xml_node node) {
  // TODO
}

ElementIdentifier
OpenDocument::new_element_(const pugi::xml_node node, const ElementType type,
                           const ElementIdentifier parent,
                           const ElementIdentifier previous_sibling) {
  Element element;
  element.node = node;
  element.type = type;
  element.parent = parent;
  element.previous_sibling = previous_sibling;

  m_elements.push_back(element);
  ElementIdentifier result = m_elements.size();

  if (parent && !previous_sibling) {
    element_(parent)->first_child = result;
  }
  if (previous_sibling) {
    element_(previous_sibling)->next_sibling = result;
  }

  return result;
}

OpenDocument::Element *
OpenDocument::element_(const ElementIdentifier element_id) {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

const OpenDocument::Element *
OpenDocument::element_(const ElementIdentifier element_id) const {
  if (!element_id) {
    return nullptr;
  }
  return &m_elements[element_id.id - 1];
}

bool OpenDocument::editable() const noexcept { return true; }

bool OpenDocument::savable(bool encrypted) const noexcept { return !encrypted; }

void OpenDocument::save(const common::Path &path) const {
  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_filesystem->is_file("mimetype")) {
    archive.insert_file(std::end(archive), "mimetype",
                        m_filesystem->open("mimetype"), 0);
  }

  for (auto walker = m_filesystem->file_walker("/"); !walker->end();
       walker->next()) {
    auto p = walker->path();
    if (p == "mimetype") {
      continue;
    }
    if (m_filesystem->is_directory(p)) {
      archive.insert_directory(std::end(archive), p);
      continue;
    }
    if (p == "content.xml") {
      // TODO stream
      std::stringstream out;
      m_content_xml.print(out);
      auto tmp = std::make_shared<common::MemoryFile>(out.str());
      archive.insert_file(std::end(archive), p, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), p, m_filesystem->open(p));
  }

  std::ofstream ostream(path.path());
  archive.save(ostream);
}

void OpenDocument::save(const common::Path &path, const char *password) const {
  // TODO throw if not savable
  throw UnsupportedOperation();
}

DocumentType OpenDocument::document_type() const noexcept {
  return m_document_type;
}

ElementIdentifier OpenDocument::root_element() const { return m_root; }

ElementIdentifier OpenDocument::first_entry_element() const {
  return 0; // TODO
}

ElementType
OpenDocument::element_type(const ElementIdentifier element_id) const {
  if (!element_id) {
    return ElementType::NONE;
  }
  return element_(element_id)->type;
}

ElementIdentifier
OpenDocument::element_parent(const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->parent;
}

ElementIdentifier
OpenDocument::element_first_child(const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->first_child;
}

ElementIdentifier OpenDocument::element_previous_sibling(
    const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->previous_sibling;
}

ElementIdentifier
OpenDocument::element_next_sibling(const ElementIdentifier element_id) const {
  if (!element_id) {
    return {};
  }
  return element_(element_id)->next_sibling;
}

std::any OpenDocument::element_property(const ElementIdentifier element_id,
                                        const ElementProperty property) const {
  if (property == ElementProperty::IMAGE_FILE) {
    if (!element_id) {
      return {};
    }

    if (element_(element_id)->type != ElementType::IMAGE) {
      return {};
    }

    // TODO use odf internal check
    if (!std::any_cast<bool>(
            element_property(element_id, ElementProperty::IMAGE_INTERNAL))) {
      // TODO support external files
      throw std::runtime_error("not internal image");
    }

    const common::Path path = std::any_cast<const char *>(
        element_property(element_id, ElementProperty::HREF));
    return m_filesystem->open(path);
  }

  return {}; // TODO
}

void OpenDocument::set_element_property(const ElementIdentifier element_id,
                                        const ElementProperty property,
                                        const std::any &value) const {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::Table>
OpenDocument::table(const ElementIdentifier element_id) const {
  return m_tables.at(element_id);
}

} // namespace odr::internal::odf
