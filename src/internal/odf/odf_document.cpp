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
#include <unordered_map>

namespace odr::internal::odf {

namespace {
class PropertyRegistry {
public:
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
    std::function<std::any(pugi::xml_node node)> get;
  };

  std::unordered_map<ElementType, std::unordered_map<ElementProperty, Entry>>
      m_registry;

  PropertyRegistry() {
    // TODO root

    default_register_(ElementType::SLIDE, ElementProperty::NAME, "draw:name");

    default_register_(ElementType::SHEET, ElementProperty::NAME, "table:name");

    default_register_(ElementType::PAGE, ElementProperty::NAME, "draw:name");

    register_(ElementType::TEXT, ElementProperty::TEXT,
              [](const pugi::xml_node node) { return node.text().get(); });

    default_register_(ElementType::LINK, ElementProperty::HREF, "xlink:href");

    default_register_(ElementType::BOOKMARK, ElementProperty::NAME,
                      "text:name");

    default_register_(ElementType::FRAME, ElementProperty::WIDTH, "svg:width");
    default_register_(ElementType::FRAME, ElementProperty::HEIGHT,
                      "svg:height");
    default_register_(ElementType::FRAME, ElementProperty::Z_INDEX,
                      "draw:z-index");

    register_(ElementType::IMAGE, ElementProperty::IMAGE_INTERNAL,
              [](const pugi::xml_node node) { return false; }); // TODO
    register_(ElementType::IMAGE, ElementProperty::HREF,
              [](const pugi::xml_node node) { return ""; }); // TODO
    register_(ElementType::IMAGE, ElementProperty::IMAGE_FILE,
              [](const pugi::xml_node node) {
                return std::shared_ptr<ImageFile>();
              }); // TODO

    default_register_(ElementType::RECT, ElementProperty::X, "svg:x");
    default_register_(ElementType::RECT, ElementProperty::Y, "svg:y");
    default_register_(ElementType::RECT, ElementProperty::WIDTH, "svg:width");
    default_register_(ElementType::RECT, ElementProperty::HEIGHT, "svg:height");

    default_register_(ElementType::LINE, ElementProperty::X1, "svg:x1");
    default_register_(ElementType::LINE, ElementProperty::Y1, "svg:y1");
    default_register_(ElementType::LINE, ElementProperty::X2, "svg:x2");
    default_register_(ElementType::LINE, ElementProperty::Y2, "svg:y2");

    default_register_(ElementType::CIRCLE, ElementProperty::X, "svg:x");
    default_register_(ElementType::CIRCLE, ElementProperty::Y, "svg:y");
    default_register_(ElementType::CIRCLE, ElementProperty::WIDTH, "svg:width");
    default_register_(ElementType::CIRCLE, ElementProperty::HEIGHT,
                      "svg:height");
  }

  void register_(const ElementType element, const ElementProperty property,
                 std::function<std::any(pugi::xml_node node)> get) {
    m_registry[element][property].get = get;
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
};
} // namespace

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

  m_style = Style(*this);
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

std::unordered_map<ElementProperty, std::any>
OpenDocument::element_properties(const ElementIdentifier element_id) const {
  std::unordered_map<ElementProperty, std::any> result;

  const Element *element = element_(element_id);
  if (element == nullptr) {
    throw std::runtime_error("element not found");
  }

  auto style_properties = m_style.element_style(element_id);
  result.insert(std::begin(style_properties), std::end(style_properties));

  PropertyRegistry::instance().resolve_properties(element->type, element->node,
                                                  result);

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
