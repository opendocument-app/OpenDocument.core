#include <fstream>
#include <internal/abstract/filesystem.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_table.h>
#include <internal/util/map_util.h>
#include <internal/util/xml_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace odr::internal::odf {

class OpenDocument::PropertyRegistry final {
public:
  using Get = std::function<std::any(pugi::xml_node node)>;

  static PropertyRegistry &instance() {
    static PropertyRegistry instance;
    return instance;
  }

  void
  resolve_properties(const ElementType element, const pugi::xml_node node,
                     std::unordered_map<ElementProperty, std::any> &result) {
    auto it = m_registry.find(element);
    if (it == std::end(m_registry)) {
      return;
    }
    for (auto &&p : it->second) {
      auto value = p.second.get(node);
      if (value.has_value()) {
        result[p.first] = value;
      }
    }
  }

private:
  struct Entry {
    Get get;
  };

  std::unordered_map<ElementType, std::unordered_map<ElementProperty, Entry>>
      m_registry;

  PropertyRegistry() {
    static auto text_style_attribute = "text:style-name";
    static auto table_style_attribute = "table:style-name";
    static auto draw_style_attribute = "draw:style-name";
    static auto draw_master_page_attribute = "draw:master-page-name";

    default_register_(ElementType::SLIDE, ElementProperty::NAME, "draw:name");
    default_register_(ElementType::SLIDE, ElementProperty::STYLE_NAME,
                      draw_style_attribute);
    default_register_(ElementType::SLIDE, ElementProperty::MASTER_PAGE_NAME,
                      draw_master_page_attribute);

    default_register_(ElementType::SHEET, ElementProperty::NAME, "table:name");

    default_register_(ElementType::PAGE, ElementProperty::NAME, "draw:name");
    default_register_(ElementType::PAGE, ElementProperty::STYLE_NAME,
                      draw_style_attribute);
    default_register_(ElementType::PAGE, ElementProperty::MASTER_PAGE_NAME,
                      draw_master_page_attribute);

    register_text_();

    default_register_(ElementType::PARAGRAPH, ElementProperty::STYLE_NAME,
                      text_style_attribute);

    default_register_(ElementType::SPAN, ElementProperty::STYLE_NAME,
                      text_style_attribute);

    default_register_(ElementType::LINK, ElementProperty::HREF, "xlink:href");
    default_register_(ElementType::LINK, ElementProperty::STYLE_NAME,
                      text_style_attribute);

    default_register_(ElementType::BOOKMARK, ElementProperty::NAME,
                      "text:name");

    default_register_(ElementType::TABLE, ElementProperty::STYLE_NAME,
                      table_style_attribute);

    default_register_(ElementType::FRAME, ElementProperty::ANCHOR_TYPE,
                      "text:anchor-type");
    default_register_(ElementType::FRAME, ElementProperty::X, "svg:x");
    default_register_(ElementType::FRAME, ElementProperty::Y, "svg:y");
    default_register_(ElementType::FRAME, ElementProperty::WIDTH, "svg:width");
    default_register_(ElementType::FRAME, ElementProperty::HEIGHT,
                      "svg:height");
    default_register_(ElementType::FRAME, ElementProperty::Z_INDEX,
                      "draw:z-index");

    default_register_(ElementType::IMAGE, ElementProperty::HREF, "xlink:href");

    default_register_(ElementType::RECT, ElementProperty::X, "svg:x");
    default_register_(ElementType::RECT, ElementProperty::Y, "svg:y");
    default_register_(ElementType::RECT, ElementProperty::WIDTH, "svg:width");
    default_register_(ElementType::RECT, ElementProperty::HEIGHT, "svg:height");
    default_register_(ElementType::RECT, ElementProperty::STYLE_NAME,
                      draw_style_attribute);

    default_register_(ElementType::LINE, ElementProperty::X1, "svg:x1");
    default_register_(ElementType::LINE, ElementProperty::Y1, "svg:y1");
    default_register_(ElementType::LINE, ElementProperty::X2, "svg:x2");
    default_register_(ElementType::LINE, ElementProperty::Y2, "svg:y2");
    default_register_(ElementType::LINE, ElementProperty::STYLE_NAME,
                      draw_style_attribute);

    default_register_(ElementType::CIRCLE, ElementProperty::X, "svg:x");
    default_register_(ElementType::CIRCLE, ElementProperty::Y, "svg:y");
    default_register_(ElementType::CIRCLE, ElementProperty::WIDTH, "svg:width");
    default_register_(ElementType::CIRCLE, ElementProperty::HEIGHT,
                      "svg:height");
    default_register_(ElementType::CIRCLE, ElementProperty::STYLE_NAME,
                      draw_style_attribute);

    default_register_(ElementType::CUSTOM_SHAPE, ElementProperty::X, "svg:x");
    default_register_(ElementType::CUSTOM_SHAPE, ElementProperty::Y, "svg:y");
    default_register_(ElementType::CUSTOM_SHAPE, ElementProperty::WIDTH,
                      "svg:width");
    default_register_(ElementType::CUSTOM_SHAPE, ElementProperty::HEIGHT,
                      "svg:height");
    default_register_(ElementType::CUSTOM_SHAPE, ElementProperty::STYLE_NAME,
                      draw_style_attribute);
    // TODO text style
  }

  void register_(const ElementType element, const ElementProperty property,
                 Get get) {
    m_registry[element][property].get = std::move(get);
  }

  void default_register_(const ElementType element,
                         const ElementProperty property,
                         const char *attribute_name) {
    register_(element, property, [attribute_name](const pugi::xml_node node) {
      auto attribute = node.attribute(attribute_name);
      if (!attribute) {
        return std::any();
      }
      return std::any(attribute.value());
    });
  }

  void register_text_() {
    register_(ElementType::TEXT, ElementProperty::TEXT,
              [](const pugi::xml_node node) {
                if (node.type() == pugi::node_pcdata) {
                  return std::any(node.text().as_string());
                }

                const std::string element = node.name();
                if (element == "text:s") {
                  const auto count = node.attribute("text:c").as_uint(1);
                  return std::any(std::string(count, ' '));
                } else if (element == "text:tab") {
                  return std::any("\t");
                }

                // TODO this should never happen. log or throw?
                return std::any("");
              });
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

  m_style =
      Style(m_content_xml.document_element(), m_styles_xml.document_element());

  m_root = register_element_(
      m_content_xml.document_element().child("office:body").first_child(), 0,
      0);

  if (!m_root) {
    throw NoOpenDocumentFile();
  }
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
      {"text:index-title", ElementType::PARAGRAPH},
      {"table:table", ElementType::TABLE},
      {"draw:frame", ElementType::FRAME},
      {"draw:image", ElementType::IMAGE},
      {"draw:rect", ElementType::RECT},
      {"draw:line", ElementType::LINE},
      {"draw:circle", ElementType::CIRCLE},
      {"draw:custom-shape", ElementType::CUSTOM_SHAPE},
      {"office:text", ElementType::ROOT},
      {"office:presentation", ElementType::ROOT},
      {"office:spreadsheet", ElementType::ROOT},
      {"office:drawing", ElementType::ROOT},
  };

  if (!node) {
    return {};
  }

  ElementType element_type = ElementType::NONE;

  if (node.type() == pugi::node_pcdata) {
    element_type = ElementType::TEXT;
  } else if (node.type() == pugi::node_element) {
    const std::string element = node.name();

    if (element == "text:table-of-content") {
      return register_children_(node.child("text:index-body"), parent,
                                previous_sibling)
          .second;
    } else if (element == "text:index-title") {
      // not sure what else to do with this tag
      return register_children_(node, parent, previous_sibling).second;
    } else if (element == "draw:g") {
      // drawing group not supported
      return register_children_(node, parent, previous_sibling).second;
    } else if ((m_document_type == DocumentType::PRESENTATION) &&
               (element == "draw:page")) {
      element_type = ElementType::SLIDE;
    } else if ((m_document_type == DocumentType::SPREADSHEET) &&
               (element == "table:table") && (parent == m_root)) {
      auto sheet =
          new_element_(node, ElementType::SHEET, parent, previous_sibling);
      return register_element_(node, sheet, previous_sibling);
    } else if ((m_document_type == DocumentType::DRAWING) &&
               (element == "draw:page")) {
      element_type = ElementType::PAGE;
    } else {
      util::map::lookup_map(element_type_table, element, element_type);
    }
  }

  if (element_type == ElementType::NONE) {
    // TODO log node
    return {};
  }

  auto new_element = new_element_(node, element_type, parent, previous_sibling);

  if (element_type == ElementType::TABLE) {
    post_register_table_(new_element, node);
  } else if (element_type == ElementType::SLIDE) {
    post_register_slide_(new_element, node);
  } else {
    register_children_(node, new_element, {});
  }

  return new_element;
}

std::pair<ElementIdentifier, ElementIdentifier>
OpenDocument::register_children_(const pugi::xml_node node,
                                 const ElementIdentifier parent,
                                 ElementIdentifier previous_sibling) {
  ElementIdentifier first_element;

  for (auto &&child_node : node) {
    const ElementIdentifier child =
        register_element_(child_node, parent, previous_sibling);
    if (!child) {
      continue;
    }
    if (!first_element) {
      first_element = child;
    }
    previous_sibling = child;
  }

  return {first_element, previous_sibling};
}

void OpenDocument::post_register_table_(const ElementIdentifier element,
                                        const pugi::xml_node node) {
  m_tables[element] = std::make_shared<Table>(*this, node);
}

void OpenDocument::post_register_slide_(const ElementIdentifier element,
                                        const pugi::xml_node node) {
  ElementIdentifier inner_previous_sibling;
  if (auto master_page_name_attr = node.attribute("draw:master-page-name")) {
    auto master_page_node =
        m_style.master_page_node(master_page_name_attr.value());
    for (auto &&child_node : master_page_node) {
      if (child_node.attribute("presentation:placeholder").as_bool()) {
        continue;
      }
      auto child =
          register_element_(child_node, element, inner_previous_sibling);
      if (!child) {
        continue;
      }
      inner_previous_sibling = child;
    }
  }
  register_children_(node, element, inner_previous_sibling);
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

bool OpenDocument::savable(const bool encrypted) const noexcept {
  return !encrypted;
}

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

std::shared_ptr<abstract::ReadableFilesystem>
OpenDocument::files() const noexcept {
  return m_filesystem;
}

ElementIdentifier OpenDocument::root_element() const { return m_root; }

ElementIdentifier OpenDocument::first_entry_element() const {
  return element_first_child(m_root);
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

std::unordered_map<ElementProperty, std::any>
OpenDocument::element_properties(const ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  const Element *element = element_(element_id);
  if (element == nullptr) {
    throw std::runtime_error("element not found");
  }

  if (element->type == ElementType::ROOT) {
    auto style_properties = m_style.resolve_master_page(
        element->type, m_style.first_master_page().value());
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  PropertyRegistry::instance().resolve_properties(element->type, element->node,
                                                  result);

  if (auto style_name_it = result.find(ElementProperty::STYLE_NAME);
      style_name_it != std::end(result)) {
    auto style_name = std::any_cast<const char *>(style_name_it->second);
    auto style_properties = m_style.resolve_style(element->type, style_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  // TODO this check does not need to happen all the time
  if (auto master_page_name_it = result.find(ElementProperty::MASTER_PAGE_NAME);
      master_page_name_it != std::end(result)) {
    auto master_page_name =
        std::any_cast<const char *>(master_page_name_it->second);
    auto style_properties =
        m_style.resolve_master_page(element->type, master_page_name);
    result.insert(std::begin(style_properties), std::end(style_properties));
  }

  return result;
}

void OpenDocument::update_element_properties(
    const ElementIdentifier element_id,
    std::unordered_map<ElementProperty, std::any> properties) const {
  throw UnsupportedOperation();
}

std::shared_ptr<abstract::Table>
OpenDocument::table(const ElementIdentifier element_id) const {
  return m_tables.at(element_id);
}

} // namespace odr::internal::odf
