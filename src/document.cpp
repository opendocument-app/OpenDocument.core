#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/common/table_range.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>

namespace odr {

namespace {
std::optional<bool> property_value_to_bool(const std::any &value) {
  if (!value.has_value()) {
    return {};
  }
  if (value.type() == typeid(bool)) {
    return std::any_cast<bool>(value);
  }
  throw std::runtime_error("conversion to bool failed");
}

std::optional<std::uint32_t> property_value_to_uint32(const std::any &value) {
  if (!value.has_value()) {
    return {};
  }
  if (value.type() == typeid(std::uint32_t)) {
    return std::any_cast<std::uint32_t>(value);
  }
  throw std::runtime_error("conversion to uint32 failed");
}

std::optional<std::string> property_value_to_string(const std::any &value) {
  if (!value.has_value()) {
    return {};
  }
  if (value.type() == typeid(std::string)) {
    return std::any_cast<std::string>(value);
  } else if (value.type() == typeid(const char *)) {
    return std::any_cast<const char *>(value);
  }
  throw std::runtime_error("conversion to string failed");
}
} // namespace

Document::Document(std::shared_ptr<internal::abstract::Document> document)
    : m_impl{std::move(document)} {
  if (!m_impl) {
    throw std::runtime_error("document is null");
  }
}

bool Document::editable() const noexcept { return m_impl->editable(); }

bool Document::savable(const bool encrypted) const noexcept {
  return m_impl->savable(encrypted);
}

void Document::save(const std::string &path) const { m_impl->save(path); }

void Document::save(const std::string &path,
                    const std::string &password) const {
  m_impl->save(path, password.c_str());
}

DocumentType Document::document_type() const noexcept {
  return m_impl->document_type();
}

DocumentCursor Document::root_element() const {
  return DocumentCursor(m_impl->root_element());
}

DocumentCursor::DocumentCursor(
    std::shared_ptr<internal::abstract::DocumentCursor> impl)
    : m_impl{std::move(impl)} {
  // TODO throw if nullptr
}

bool DocumentCursor::operator==(const DocumentCursor &rhs) const {
  return m_impl->operator==(*rhs.m_impl);
}

bool DocumentCursor::operator!=(const DocumentCursor &rhs) const {
  return m_impl->operator!=(*rhs.m_impl);
}

std::string DocumentCursor::document_path() const {
  return m_impl->document_path();
}

ElementType DocumentCursor::element_type() const {
  return m_impl->element_type();
}

ElementPropertySet DocumentCursor::element_properties() const {
  return ElementPropertySet(m_impl->element_properties());
}

TableElement DocumentCursor::table() const {
  return TableElement(m_impl->table());
}

ImageElement DocumentCursor::image() const {
  return ImageElement(m_impl->image());
}

bool DocumentCursor::move_to_parent() { return m_impl->move_to_parent(); }

bool DocumentCursor::move_to_first_child() {
  return m_impl->move_to_first_child();
}

bool DocumentCursor::move_to_previous_sibling() {
  return m_impl->move_to_previous_sibling();
}

bool DocumentCursor::move_to_next_sibling() {
  return m_impl->move_to_next_sibling();
}

void DocumentCursor::for_each_child(ChildVisitor visitor) {
  std::uint32_t i = 0;
  if (!move_to_first_child()) {
    return;
  }
  while (true) {
    visitor(*this, i);
    if (!move_to_next_sibling()) {
      break;
    }
    ++i;
  }
  move_to_parent();
}

TableDimensions::TableDimensions() = default;

TableDimensions::TableDimensions(const std::uint32_t rows,
                                 const std::uint32_t columns)
    : rows{rows}, columns{columns} {}

TableElement::TableElement(internal::abstract::TableElement *impl)
    : m_impl{impl} {}

TableElement::operator bool() const { return m_impl != nullptr; }

TableDimensions TableElement::dimensions() const {
  return m_impl->dimensions();
}

ImageElement::ImageElement(internal::abstract::ImageElement *impl)
    : m_impl{impl} {}

ImageElement::operator bool() const { return m_impl != nullptr; }

bool ImageElement::internal() const { return m_impl->internal(); }

std::optional<File> ImageElement::image_file() const {
  return m_impl->image_file();
}

ElementPropertySet::ElementPropertySet(
    std::unordered_map<ElementProperty, std::any> properties)
    : m_properties{std::move(properties)} {}

std::any ElementPropertySet::get(const ElementProperty property) const {
  auto it = m_properties.find(property);
  if (it == std::end(m_properties)) {
    return {};
  }
  return it->second;
}

std::optional<std::string>
ElementPropertySet::get_string(const ElementProperty property) const {
  return property_value_to_string(get(property));
}

std::optional<std::uint32_t>
ElementPropertySet::get_uint32(const ElementProperty property) const {
  return property_value_to_uint32(get(property));
}

std::optional<bool>
ElementPropertySet::get_bool(const ElementProperty property) const {
  return property_value_to_bool(get(property));
}

} // namespace odr
