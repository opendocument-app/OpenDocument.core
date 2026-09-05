#include <odr/internal/odf/odf_document.hpp>

#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/table_cursor.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/odf/odf_chart.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>
#include <odr/internal/odf/odf_geometry.hpp>
#include <odr/internal/odf/odf_list.hpp>
#include <odr/internal/odf/odf_parser.hpp>
#include <odr/internal/odf/odf_table.hpp>
#include <odr/internal/util/document_util.hpp>
#include <odr/internal/util/string_util.hpp>
#include <odr/internal/xml/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <cstring>
#include <mutex>
#include <ostream>
#include <sstream>
#include <unordered_map>

namespace odr::internal::odf {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry);
}

Document::Document(const FileType file_type, const DocumentType document_type,
                   std::shared_ptr<abstract::ReadableFilesystem> files,
                   const EncryptionState encryption_state)
    : internal::Document(file_type, document_type, std::move(files),
                         encryption_state) {
  m_content_xml = xml::parse(*m_files, AbsPath("/content.xml"));

  if (m_files->exists(AbsPath("/styles.xml"))) {
    m_styles_xml = xml::parse(*m_files, AbsPath("/styles.xml"));
  }

  init_(m_content_xml.document_element(), m_styles_xml.document_element());
}

Document::Document(const FileType file_type, const DocumentType document_type,
                   pugi::xml_document flat_xml)
    : internal::Document(file_type, document_type, nullptr),
      m_content_xml{std::move(flat_xml)} {
  // content and styles live under the one root
  init_(m_content_xml.document_element(), m_content_xml.document_element());
}

void Document::init_(const pugi::xml_node content_root,
                     const pugi::xml_node styles_root) {
  m_root_element = parse_tree(m_element_registry,
                              content_root.child("office:body").first_child());

  m_style_registry = StyleRegistry(*this, content_root, styles_root);

  resolve_list_numbering(m_element_registry, m_style_registry, m_root_element);

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

ElementRegistry &Document::element_registry() { return m_element_registry; }

StyleRegistry &Document::style_registry() { return m_style_registry; }

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

const StyleRegistry &Document::style_registry() const {
  return m_style_registry;
}

bool Document::is_editable() const noexcept {
  // TODO fix spreadsheet editability
  return m_document_type == DocumentType::text ||
         m_document_type == DocumentType::presentation ||
         m_document_type == DocumentType::drawing;
}

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted && !is_decrypted();
}

void Document::save(std::ostream &out) const {
  if (!is_savable(false)) {
    throw UnsupportedOperation();
  }

  // no package to rebuild: a flat document is the one tree, and `save` puts
  // back the declaration the parse dropped
  if (m_files == nullptr) {
    m_content_xml.save(out, "", pugi::format_raw);
    return;
  }

  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  // `mimetype` has to be the first file and uncompressed
  if (m_files->is_file(AbsPath("/mimetype"))) {
    archive.insert_file(std::end(archive), RelPath("mimetype"),
                        m_files->open(AbsPath("/mimetype")), 0);
  }

  for (auto walker = m_files->file_walker(AbsPath("/")); !walker->end();
       walker->next()) {
    const AbsPath &abs_path = walker->path();
    RelPath rel_path = abs_path.rebase(AbsPath("/"));
    if (abs_path == Path("/mimetype")) {
      continue;
    }
    if (walker->is_directory()) {
      archive.insert_directory(std::end(archive), rel_path);
      continue;
    }
    if (abs_path == Path("/content.xml")) {
      // TODO stream
      std::stringstream content;
      m_content_xml.print(content, "", pugi::format_raw);
      auto tmp = std::make_shared<MemoryFile>(content.str());
      archive.insert_file(std::end(archive), rel_path, tmp);
      continue;
    }
    archive.insert_file(std::end(archive), rel_path, m_files->open(abs_path));
  }

  archive.save(out);
}

void Document::save(std::ostream & /*out*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

namespace {

/// Absent or unparsable geometry yields nullopt rather than throwing.
std::optional<Measure> read_measure(const pugi::xml_attribute attribute) {
  if (!attribute) {
    return std::nullopt;
  }
  try {
    return Measure(attribute.value());
  } catch (...) { // NOLINT(bugprone-empty-catch)
    return std::nullopt;
  }
}

/// Same, for attributes the API reports unconditionally.
Measure read_measure_or_zero(const pugi::xml_attribute attribute) {
  return read_measure(attribute).value_or(Measure(0, DynamicUnit()));
}

/// The unit an `svg:d` with no view box is written in (19.180).
Measure hundredth_millimetres(const double value) {
  return Measure(value / 100.0, DynamicUnit("mm"));
}

/// A `draw:connector` states no box of its own, so its path is what places it.
std::optional<DrawingPath> connector_box(const pugi::xml_node node) {
  if (std::strcmp(node.name(), "draw:connector") != 0) {
    return {};
  }
  return read_path(node);
}

class ElementAdapter final : public abstract::ElementAdapter,
                             public abstract::TextRootAdapter,
                             public abstract::SlideAdapter,
                             public abstract::PageAdapter,
                             public abstract::SheetAdapter,
                             public abstract::SheetCellAdapter,
                             public abstract::MasterPageAdapter,
                             public abstract::LineBreakAdapter,
                             public abstract::ParagraphAdapter,
                             public abstract::SpanAdapter,
                             public abstract::TextAdapter,
                             public abstract::LinkAdapter,
                             public abstract::BookmarkAdapter,
                             public abstract::ListAdapter,
                             public abstract::ListItemAdapter,
                             public abstract::TableAdapter,
                             public abstract::TableColumnAdapter,
                             public abstract::TableRowAdapter,
                             public abstract::TableCellAdapter,
                             public abstract::FrameAdapter,
                             public abstract::RectAdapter,
                             public abstract::LineAdapter,
                             public abstract::CircleAdapter,
                             public abstract::CustomShapeAdapter,
                             public abstract::ImageAdapter {
public:
  ElementAdapter(const Document &document, ElementRegistry &registry)
      : m_document(&document), m_registry(&registry) {}

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
    return true;
  }
  [[nodiscard]] bool element_is_self_locatable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }
  [[nodiscard]] bool
  element_is_editable(const ElementIdentifier element_id) const override {
    const ElementRegistry::Element &element =
        m_registry->element_at(element_id);
    if (element.type == ElementType::sheet_cell) {
      return !m_registry->sheet_cell_element_at(element_id).is_repeated;
    }
    if (element.parent_id != null_element_id) {
      return element_is_editable(element.parent_id);
    }
    return true;
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

  [[nodiscard]] const TextRootAdapter *
  text_root_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::root ? this : nullptr;
  }
  [[nodiscard]] const SlideAdapter *
  slide_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::slide ? this : nullptr;
  }
  [[nodiscard]] const PageAdapter *
  page_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::page ? this : nullptr;
  }
  [[nodiscard]] const SheetAdapter *
  sheet_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::sheet ? this : nullptr;
  }
  [[nodiscard]] const SheetCellAdapter *
  sheet_cell_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::sheet_cell ? this : nullptr;
  }
  [[nodiscard]] const MasterPageAdapter *
  master_page_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::master_page ? this
                                                                : nullptr;
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
  [[nodiscard]] const BookmarkAdapter *
  bookmark_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::bookmark ? this : nullptr;
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
  [[nodiscard]] const FrameAdapter *
  frame_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::frame ? this : nullptr;
  }
  [[nodiscard]] const RectAdapter *
  rect_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::rect ? this : nullptr;
  }
  [[nodiscard]] const LineAdapter *
  line_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::line ? this : nullptr;
  }
  [[nodiscard]] const CircleAdapter *
  circle_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::circle ? this : nullptr;
  }
  [[nodiscard]] const CustomShapeAdapter *
  custom_shape_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::custom_shape ? this
                                                                 : nullptr;
  }
  [[nodiscard]] const ImageAdapter *
  image_adapter(const ElementIdentifier element_id) const override {
    return element_type(element_id) == ElementType::image ? this : nullptr;
  }

  [[nodiscard]] PageLayout
  text_root_page_layout(const ElementIdentifier element_id) const override {
    if (const ElementIdentifier master_page_id =
            text_root_first_master_page(element_id);
        master_page_id != null_element_id) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  static bool is_anchored_frame(const pugi::xml_node node) {
    return std::strcmp(node.name(), "draw:frame") == 0;
  }

  /// Whether laying this out would put a word on the page; a frame is anchored,
  /// not written.
  static bool writes_text(const pugi::xml_node node) {
    for (const pugi::xml_node child : node.children()) {
      if (child.type() == pugi::node_pcdata &&
          !std::string_view(child.value()).empty()) {
        return true;
      }
      if (!is_anchored_frame(child) && writes_text(child)) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] ElementIdentifier text_root_first_master_page(
      const ElementIdentifier element_id) const override {
    // A paragraph may name the master page its page uses (20.283). One page box
    // is all we lay out, so only a name ahead of every written word counts.
    for (const pugi::xml_node child : get_node(element_id).children()) {
      if (is_anchored_frame(child)) {
        continue;
      }
      if (const ElementIdentifier master_page_id =
              m_document->style_registry().master_page_of_style(
                  child.attribute("text:style-name").value());
          master_page_id != null_element_id) {
        return master_page_id;
      }
      if (writes_text(child)) {
        break;
      }
    }
    return m_document->style_registry().first_master_page();
  }

  [[nodiscard]] PageLayout
  slide_page_layout(const ElementIdentifier element_id) const override {
    if (const ElementIdentifier master_page_id = slide_master_page(element_id);
        master_page_id != null_element_id) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  [[nodiscard]] ElementIdentifier
  slide_master_page(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute master_page_name_attr =
            node.attribute("draw:master-page-name");
        master_page_name_attr) {
      return m_document->style_registry().master_page(
          master_page_name_attr.value());
    }
    return {};
  }
  [[nodiscard]] std::string
  slide_name(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("draw:name").value();
  }

  [[nodiscard]] PageLayout
  page_layout(const ElementIdentifier element_id) const override {
    if (const ElementIdentifier master_page_id = page_master_page(element_id);
        master_page_id != null_element_id) {
      return master_page_page_layout(master_page_id);
    }
    return {};
  }
  [[nodiscard]] ElementIdentifier
  page_master_page(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute master_page_name_attr =
            node.attribute("draw:master-page-name");
        master_page_name_attr) {
      return m_document->style_registry().master_page(
          master_page_name_attr.value());
    }
    return {};
  }
  [[nodiscard]] std::string
  page_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("draw:name").value();
  }

  [[nodiscard]] std::string
  sheet_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("table:name").value();
  }
  [[nodiscard]] PageLayout
  sheet_page_layout(const ElementIdentifier element_id) const override {
    // The table style names the master page (20.383); a sheet that names none
    // takes the first.
    ElementIdentifier master_page_id =
        m_document->style_registry().master_page_of_style(
            get_node(element_id).attribute("table:style-name").value());
    if (master_page_id == null_element_id) {
      master_page_id = m_document->style_registry().first_master_page();
    }
    if (master_page_id == null_element_id) {
      return {};
    }
    return master_page_page_layout(master_page_id);
  }
  [[nodiscard]] TableDimensions
  sheet_dimensions(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).dimensions;
  }
  [[nodiscard]] TableDimensions
  sheet_content(const ElementIdentifier element_id,
                const std::optional<TableDimensions> range) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;

    TableCursor cursor;
    for_each_table_row(node, [&](const pugi::xml_node row) {
      const auto rows_repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      cursor.add_row(rows_repeated);

      for (auto cell : row.children("table:table-cell")) {
        const auto columns_repeated =
            cell.attribute("table:number-columns-repeated").as_uint(1);
        const auto colspan =
            cell.attribute("table:number-columns-spanned").as_uint(1);
        const auto rowspan =
            cell.attribute("table:number-rows-spanned").as_uint(1);
        cursor.add_cell(colspan, rowspan, columns_repeated);

        const std::uint32_t new_rows = cursor.row();
        const std::uint32_t new_cols =
            std::max(result.columns, cursor.column());
        if (cell.first_child() &&
            (!range || (new_rows < range->rows && new_cols < range->columns))) {
          result.rows = new_rows;
          result.columns = new_cols;
        }
      }
    });

    return result;
  }
  [[nodiscard]] ElementIdentifier
  sheet_cell(const ElementIdentifier element_id, const std::uint32_t column,
             const std::uint32_t row) const override {
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element_at(element_id);
    if (const ElementRegistry::Sheet::Cell *sheet_cell =
            sheet_registry.cell(column, row);
        sheet_cell != nullptr) {
      return sheet_cell->element_id;
    }
    return {};
  }
  [[nodiscard]] ElementIdentifier
  sheet_first_shape(const ElementIdentifier element_id) const override {
    return m_registry->sheet_element_at(element_id).first_shape_id;
  }
  [[nodiscard]] TableStyle
  sheet_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_style;
  }
  [[nodiscard]] TableColumnStyle
  sheet_column_style(const ElementIdentifier element_id,
                     const std::uint32_t column) const override {
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element_at(element_id);
    const pugi::xml_node column_node = sheet_registry.column_node(column);
    if (const pugi::xml_attribute attr =
            column_node.attribute("table:style-name");
        attr) {
      if (const Style *style = m_document->style_registry().style(attr.value());
          style != nullptr) {
        return style->resolved().table_column_style;
      }
    }
    return {};
  }
  [[nodiscard]] TableRowStyle
  sheet_row_style(const ElementIdentifier element_id,
                  const std::uint32_t row) const override {
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element_at(element_id);
    const pugi::xml_node row_node = sheet_registry.row_node(row);
    if (const pugi::xml_attribute attr =
            row_node.attribute("table:style-name")) {
      if (const Style *style = m_document->style_registry().style(attr.value());
          style != nullptr) {
        return style->resolved().table_row_style;
      }
    }
    return {};
  }
  [[nodiscard]] TableCellStyle
  sheet_cell_style(const ElementIdentifier element_id,
                   const std::uint32_t column,
                   const std::uint32_t row) const override {
    const ElementIdentifier cell_id = sheet_cell(element_id, column, row);
    return get_partial_cell_style(element_id, cell_id, {column, row})
        .table_cell_style;
  }

  [[nodiscard]] TablePosition
  sheet_cell_position(const ElementIdentifier element_id) const override {
    return m_registry->sheet_cell_element_at(element_id).position;
  }
  [[nodiscard]] bool
  sheet_cell_is_covered(const ElementIdentifier element_id) const override {
    return std::strcmp(get_node(element_id).name(),
                       "table:covered-table-cell") == 0;
  }
  [[nodiscard]] TableDimensions
  sheet_cell_span(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("table:number-rows-spanned").as_uint(1),
            node.attribute("table:number-columns-spanned").as_uint(1)};
  }
  [[nodiscard]] ValueType
  sheet_cell_value_type(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const char *value_type = node.attribute("office:value-type").value();
        std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }

  [[nodiscard]] PageLayout
  master_page_page_layout(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute =
            node.attribute("style:page-layout-name");
        attribute) {
      return m_document->style_registry().page_layout(attribute.value());
    }
    return {};
  }

  [[nodiscard]] TextStyle
  line_break_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).paragraph_style;
  }
  [[nodiscard]] TextStyle
  paragraph_text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] TextStyle
  span_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    const ElementRegistry::Text &text_element =
        m_registry->text_element_at(element_id);

    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node end = text_element.last.next_sibling();

    std::string result;
    for (pugi::xml_node node = first; node != end; node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    ElementRegistry::Element &element = m_registry->element_at(element_id);
    ElementRegistry::Text &text_element =
        m_registry->text_element_at(element_id);

    pugi::xml_node parent = get_node(element_id).parent();
    const pugi::xml_node old_first = get_node(element_id);
    const pugi::xml_node old_last = text_element.last;
    // the removal loop below invalidates `old_last`
    const pugi::xml_node old_end = old_last.next_sibling();
    pugi::xml_node new_first = old_first;
    pugi::xml_node new_last = old_last;

    const auto track = [&](const pugi::xml_node new_node) {
      if (new_first == old_first) {
        new_first = new_node;
      }
      new_last = new_node;
      return new_node;
    };
    const auto insert_pcdata = [&] {
      return track(parent.insert_child_before(pugi::xml_node_type::node_pcdata,
                                              old_first));
    };
    const auto insert_node = [&](const char *node) {
      return track(parent.insert_child_before(node, old_first));
    };

    for (const xml::StringToken &token : xml::tokenize_text(text)) {
      switch (token.type) {
      case xml::StringToken::Type::none:
        break;
      case xml::StringToken::Type::string: {
        auto text_node = insert_pcdata();
        text_node.text().set(token.string.c_str());
      } break;
      case xml::StringToken::Type::spaces: {
        auto space_node = insert_node("text:s");
        space_node.prepend_attribute("text:c").set_value(token.string.size());
      } break;
      case xml::StringToken::Type::tabs: {
        for (std::size_t i = 0; i < token.string.size(); ++i) {
          insert_node("text:tab");
        }
      } break;
      }
    }

    if (new_first == old_first) {
      // empty text still needs a live node to anchor the element to, or the
      // removal below would leave the registry pointing at freed nodes
      insert_pcdata();
    }

    element.node = new_first;
    text_element.last = new_last;

    for (pugi::xml_node node = old_first; node != old_end;) {
      const pugi::xml_node next = node.next_sibling();
      parent.remove_child(node);
      node = next;
    }
  }
  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  link_href(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("xlink:href").value();
  }

  [[nodiscard]] std::string
  bookmark_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("text:name").value();
  }

  [[nodiscard]] ListType
  list_type(const ElementIdentifier element_id) const override {
    return m_registry->list_type(element_id);
  }

  [[nodiscard]] TextStyle
  list_item_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  list_item_marker(const ElementIdentifier element_id) const override {
    return m_registry->list_marker(element_id).text;
  }

  [[nodiscard]] std::optional<std::uint32_t>
  list_item_number(const ElementIdentifier element_id) const override {
    return m_registry->list_marker(element_id).number;
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;
    TableCursor cursor;

    for_each_table_column(node, [&](const pugi::xml_node column) {
      const auto columns_repeated =
          column.attribute("table:number-columns-repeated").as_uint(1);
      cursor.add_column(columns_repeated);
    });

    result.columns = cursor.column();
    cursor = {};

    for_each_table_row(node, [&](const pugi::xml_node row) {
      const auto rows_repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      cursor.add_row(rows_repeated);
    });

    result.rows = cursor.row();

    return result;
  }
  [[nodiscard]] ElementIdentifier
  table_first_column(const ElementIdentifier element_id) const override {
    return m_registry->table_element_at(element_id).first_column_id;
  }
  [[nodiscard]] ElementIdentifier
  table_first_row(const ElementIdentifier element_id) const override {
    return element_first_child(element_id);
  }
  [[nodiscard]] TableStyle
  table_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_style;
  }

  [[nodiscard]] TableColumnStyle
  table_column_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_column_style;
  }

  [[nodiscard]] TableRowStyle
  table_row_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_row_style;
  }

  [[nodiscard]] bool
  table_cell_is_covered(const ElementIdentifier element_id) const override {
    return std::strcmp(get_node(element_id).name(),
                       "table:covered-table-cell") == 0;
  }
  [[nodiscard]] TableDimensions
  table_cell_span(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("table:number-rows-spanned").as_uint(1),
            node.attribute("table:number-columns-spanned").as_uint(1)};
  }
  [[nodiscard]] ValueType
  table_cell_value_type(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const char *value_type = node.attribute("office:value-type").value();
        std::strcmp("float", value_type) == 0) {
      return ValueType::float_number;
    }
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  table_cell_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_cell_style;
  }

  [[nodiscard]] AnchorType
  frame_anchor_type(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    const char *anchor_type = node.attribute("text:anchor-type").value();
    if (std::strcmp("as-char", anchor_type) == 0) {
      return AnchorType::as_char;
    }
    if (std::strcmp("char", anchor_type) == 0) {
      return AnchorType::at_char;
    }
    if (std::strcmp("paragraph", anchor_type) == 0) {
      return AnchorType::at_paragraph;
    }
    if (std::strcmp("page", anchor_type) == 0) {
      return AnchorType::at_page;
    }
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<Measure>
  frame_x(const ElementIdentifier element_id) const override {
    return read_measure(get_node(element_id).attribute("svg:x"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_y(const ElementIdentifier element_id) const override {
    return read_measure(get_node(element_id).attribute("svg:y"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_width(const ElementIdentifier element_id) const override {
    return read_measure(get_node(element_id).attribute("svg:width"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_height(const ElementIdentifier element_id) const override {
    return read_measure(get_node(element_id).attribute("svg:height"));
  }
  [[nodiscard]] std::optional<std::int32_t>
  frame_z_index(const ElementIdentifier element_id) const override {
    const pugi::xml_attribute attribute =
        get_node(element_id).attribute("draw:z-index");
    if (!attribute) {
      return std::nullopt;
    }
    return static_cast<std::int32_t>(attribute.as_int());
  }
  [[nodiscard]] std::optional<DrawingTransform>
  frame_transform(const ElementIdentifier element_id) const override {
    return read_transform(get_node(element_id));
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] Measure
  rect_x(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:x"));
  }
  [[nodiscard]] Measure
  rect_y(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:y"));
  }
  [[nodiscard]] Measure
  rect_width(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:width"));
  }
  [[nodiscard]] Measure
  rect_height(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:height"));
  }
  [[nodiscard]] std::optional<DrawingTransform>
  rect_transform(const ElementIdentifier element_id) const override {
    return read_transform(get_node(element_id));
  }
  [[nodiscard]] GraphicStyle
  rect_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] Measure
  line_x1(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:x1"));
  }
  [[nodiscard]] Measure
  line_y1(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:y1"));
  }
  [[nodiscard]] Measure
  line_x2(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:x2"));
  }
  [[nodiscard]] Measure
  line_y2(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:y2"));
  }
  [[nodiscard]] std::optional<DrawingTransform>
  line_transform(const ElementIdentifier element_id) const override {
    return read_transform(get_node(element_id));
  }
  [[nodiscard]] GraphicStyle
  line_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] Measure
  circle_x(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:x"));
  }
  [[nodiscard]] Measure
  circle_y(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:y"));
  }
  [[nodiscard]] Measure
  circle_width(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:width"));
  }
  [[nodiscard]] Measure
  circle_height(const ElementIdentifier element_id) const override {
    return read_measure_or_zero(get_node(element_id).attribute("svg:height"));
  }
  [[nodiscard]] std::optional<DrawingTransform>
  circle_transform(const ElementIdentifier element_id) const override {
    return read_transform(get_node(element_id));
  }
  [[nodiscard]] GraphicStyle
  circle_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] std::optional<Measure>
  custom_shape_x(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const std::optional<Measure> measure =
            read_measure(node.attribute("svg:x"))) {
      return measure;
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->x);
    }
    return {};
  }
  [[nodiscard]] std::optional<Measure>
  custom_shape_y(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const std::optional<Measure> measure =
            read_measure(node.attribute("svg:y"))) {
      return measure;
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->y);
    }
    return {};
  }
  [[nodiscard]] Measure
  custom_shape_width(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:width")) {
      return read_measure_or_zero(attribute);
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->width);
    }
    return Measure(0, DynamicUnit());
  }
  [[nodiscard]] Measure
  custom_shape_height(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_attribute attribute = node.attribute("svg:height")) {
      return read_measure_or_zero(attribute);
    }
    if (const std::optional<DrawingPath> box = connector_box(node)) {
      return hundredth_millimetres(box->height);
    }
    return Measure(0, DynamicUnit());
  }
  [[nodiscard]] std::optional<DrawingPath>
  custom_shape_path(const ElementIdentifier element_id) const override {
    return read_path(get_node(element_id));
  }
  [[nodiscard]] std::optional<DrawingTransform>
  custom_shape_transform(const ElementIdentifier element_id) const override {
    return read_transform(get_node(element_id));
  }
  [[nodiscard]] GraphicStyle
  custom_shape_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).graphic_style;
  }

  [[nodiscard]] bool
  image_is_internal(const ElementIdentifier element_id) const override {
    if (image_data(element_id)) {
      return true;
    }
    if (m_document->as_filesystem() == nullptr) {
      return false;
    }
    if (is_object(element_id)) {
      return object_file(element_id).has_value();
    }
    try {
      const AbsPath path = Path(image_href(element_id)).make_absolute();
      return m_document->as_filesystem()->is_file(path);
    } catch (...) { // NOLINT(bugprone-empty-catch): any error => not internal
    }
    return false;
  }
  [[nodiscard]] std::optional<File>
  image_file(const ElementIdentifier element_id) const override {
    if (const pugi::xml_node data = image_data(element_id)) {
      return File(std::make_shared<MemoryFile>(
          crypto::util::base64_decode(data.text().get())));
    }
    if (m_document->as_filesystem() == nullptr) {
      return std::nullopt;
    }
    if (is_object(element_id)) {
      return object_file(element_id);
    }
    const AbsPath path = Path(image_href(element_id)).make_absolute();
    return File(m_document->as_filesystem()->open(path));
  }
  [[nodiscard]] std::string
  image_href(const ElementIdentifier element_id) const override {
    // an embedded image has no path of its own, and the renderer names the
    // resource it writes after this
    if (image_data(element_id)) {
      return "image" + std::to_string(element_id);
    }
    std::string href = get_node(element_id).attribute("xlink:href").value();
    if (is_object(element_id)) {
      return replacement_href(element_id).value_or(href + "/chart.svg");
    }
    return href;
  }

private:
  const Document *m_document{nullptr};
  ElementRegistry *m_registry{nullptr};
  mutable std::mutex m_charts_mutex;
  mutable std::unordered_map<ElementIdentifier, std::optional<std::string>>
      m_charts;

  [[nodiscard]] pugi::xml_node
  get_node(const ElementIdentifier element_id) const {
    return m_registry->element_at(element_id).node;
  }

  /// The image's base64 bytes where the markup carries them itself.
  [[nodiscard]] pugi::xml_node
  image_data(const ElementIdentifier element_id) const {
    const pugi::xml_node data =
        get_node(element_id).child("office:binary-data");
    return data.text().empty() ? pugi::xml_node() : data;
  }

  [[nodiscard]] bool is_object(const ElementIdentifier element_id) const {
    return std::strcmp(get_node(element_id).name(), "draw:object") == 0;
  }

  /// The `draw:image` the producer wrote beside the object (10.4.6.2), which
  /// is what draws where the object itself cannot be read.
  [[nodiscard]] std::optional<std::string>
  replacement_href(const ElementIdentifier element_id) const {
    if (chart_svg(element_id).has_value()) {
      return {};
    }
    const pugi::xml_attribute href = get_node(element_id)
                                         .parent()
                                         .child("draw:image")
                                         .attribute("xlink:href");
    if (!href) {
      return {};
    }
    return href.value();
  }

  [[nodiscard]] std::optional<File>
  object_file(const ElementIdentifier element_id) const {
    if (const std::optional<std::string> svg = chart_svg(element_id)) {
      return File(std::make_shared<MemoryFile>(*svg));
    }
    const std::optional<std::string> replacement = replacement_href(element_id);
    if (!replacement.has_value()) {
      return std::nullopt;
    }
    try {
      const AbsPath path = Path(*replacement).make_absolute();
      return File(m_document->as_filesystem()->open(path));
    } catch (...) { // NOLINT(bugprone-empty-catch): no replacement either
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<AbsPath>
  object_part(const ElementIdentifier element_id) const {
    const char *href = get_node(element_id).attribute("xlink:href").value();
    if (href[0] == '\0' || m_document->as_filesystem() == nullptr) {
      return {};
    }
    try {
      AbsPath path = Path(href).make_absolute().join(RelPath("content.xml"));
      if (m_document->as_filesystem()->is_file(path)) {
        return path;
      }
    } catch (...) { // NOLINT(bugprone-empty-catch): no part of its own
    }
    return {};
  }

  /// The object's own part rendered, or nothing where it holds no chart. Kept,
  /// because `image_is_internal`, the resource and the `src` each ask for it.
  [[nodiscard]] const std::optional<std::string> &
  chart_svg(const ElementIdentifier element_id) const {
    const std::lock_guard lock(m_charts_mutex);
    if (const auto it = m_charts.find(element_id); it != m_charts.end()) {
      return it->second;
    }
    std::optional<std::string> result;
    if (const std::optional<AbsPath> path = object_part(element_id)) {
      try {
        const pugi::xml_document content =
            xml::parse(*m_document->as_filesystem()->open(*path));
        result = render_chart(content.document_element());
      } catch (...) { // NOLINT(bugprone-empty-catch): no chart we can read
      }
    }
    return m_charts.emplace(element_id, std::move(result)).first->second;
  }

  [[nodiscard]] static std::string get_text(const pugi::xml_node node) {
    if (node.type() == pugi::node_pcdata) {
      return node.value();
    }

    const std::string name = node.name();
    if (name == "text:s") {
      const std::size_t count = node.attribute("text:c").as_uint(1);
      return std::string(count, ' ');
    }
    if (name == "text:tab") {
      return "\t";
    }
    return "";
  }

  [[nodiscard]] const char *
  get_style_name(const ElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    for (const pugi::xml_attribute attribute : node.attributes()) {
      if (util::string::ends_with(attribute.name(), ":style-name")) {
        return attribute.value();
      }
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_partial_style(const ElementIdentifier element_id) const {
    if (const ElementRegistry::SheetCell *cell_registry =
            m_registry->sheet_cell_element(element_id);
        cell_registry != nullptr) {
      const ElementIdentifier parent_id = element_parent(element_id);
      return get_partial_cell_style(parent_id, element_id,
                                    cell_registry->position);
    }
    if (const char *style_name = get_style_name(element_id);
        style_name != nullptr) {
      if (const Style *style = m_document->style_registry().style(style_name)) {
        return style->resolved();
      }
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_intermediate_style(const ElementIdentifier element_id) const {
    const ElementIdentifier parent_id = element_parent(element_id);
    if (parent_id == null_element_id) {
      return get_partial_style(element_id);
    }
    ResolvedStyle base = get_intermediate_style(parent_id);
    base.override(get_partial_style(element_id));
    return base;
  }

  [[nodiscard]] ResolvedStyle
  get_partial_cell_style(const ElementIdentifier sheet_id,
                         const ElementIdentifier cell_id,
                         const TablePosition &position) const {
    const char *style_name = nullptr;

    if (cell_id != null_element_id) {
      if (const pugi::xml_attribute attribute =
              get_node(cell_id).attribute("table:style-name");
          attribute) {
        style_name = attribute.value();
      }
    }

    const auto [column, row] = position;
    const ElementRegistry::Sheet &sheet_registry =
        m_registry->sheet_element_at(sheet_id);

    if (style_name == nullptr) {
      const pugi::xml_node cell_node = sheet_registry.cell_node(column, row);
      if (const pugi::xml_attribute attribute =
              cell_node.attribute("table:style-name");
          attribute) {
        style_name = attribute.value();
      }
    }

    if (style_name == nullptr) {
      const pugi::xml_node row_node = sheet_registry.row_node(row);
      if (const pugi::xml_attribute attribute =
              row_node.attribute("table:default-cell-style-name");
          attribute) {
        style_name = attribute.value();
      }
    }
    if (style_name == nullptr) {
      const pugi::xml_node column_node = sheet_registry.column_node(column);
      if (const pugi::xml_attribute attribute =
              column_node.attribute("table:default-cell-style-name");
          attribute) {
        style_name = attribute.value();
      }
    }

    if (style_name != nullptr) {
      if (const Style *style = m_document->style_registry().style(style_name);
          style != nullptr) {
        return style->resolved();
      }
    }

    return {};
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(const Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::odf
