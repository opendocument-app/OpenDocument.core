#include <internal/abstract/document.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
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
  return m_impl->element()->type(m_impl.get());
}

ElementPropertySet DocumentCursor::element_properties() const {
  return ElementPropertySet(m_impl->element()->properties(m_impl.get()));
}

bool DocumentCursor::move_to_parent() {
  return m_impl->element()->move_to_parent(m_impl.get());
}

bool DocumentCursor::move_to_first_child() {
  return m_impl->element()->move_to_first_child(m_impl.get());
}

bool DocumentCursor::move_to_previous_sibling() {
  return m_impl->element()->move_to_previous_sibling(m_impl.get());
}

bool DocumentCursor::move_to_next_sibling() {
  return m_impl->element()->move_to_next_sibling(m_impl.get());
}

std::string DocumentCursor::text() const {
  auto element = m_impl->element();
  return element->text(m_impl.get())->value(m_impl.get(), element);
}

bool DocumentCursor::move_to_master_page() {
  auto element = m_impl->element();
  return element->slide(m_impl.get())
      ->move_to_master_page(m_impl.get(), element);
}

TableDimensions DocumentCursor::table_dimensions() {
  auto element = m_impl->element();
  return element->table(m_impl.get())->table_dimensions(m_impl.get(), element);
}

bool DocumentCursor::move_to_first_table_column() {
  auto element = m_impl->element();
  return element->table(m_impl.get())
      ->move_to_first_table_column(m_impl.get(), element);
}

bool DocumentCursor::move_to_first_table_row() {
  auto element = m_impl->element();
  return element->table(m_impl.get())
      ->move_to_first_table_row(m_impl.get(), element);
}

TableDimensions DocumentCursor::table_cell_span() {
  auto element = m_impl->element();
  return element->table_cell(m_impl.get())
      ->table_cell_span(m_impl.get(), element);
}

bool DocumentCursor::image_internal() const {
  auto element = m_impl->element();
  return element->image(m_impl.get())->image_internal(m_impl.get(), element);
}

std::optional<File> DocumentCursor::image_file() const {
  auto element = m_impl->element();
  return element->image(m_impl.get())->image_file(m_impl.get(), element);
}

void DocumentCursor::for_each_child(const ChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_column(const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_column()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_row(const ConditionalChildVisitor &visitor) {
  if (!move_to_first_table_row()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_cell(const ConditionalChildVisitor &visitor) {
  if (!move_to_first_child()) {
    return;
  }
  for_each_(visitor);
}

void DocumentCursor::for_each_(const ChildVisitor &visitor) {
  std::uint32_t i = 0;
  while (true) {
    visitor(*this, i);
    if (!move_to_next_sibling()) {
      break;
    }
    ++i;
  }
  move_to_parent();
}

void DocumentCursor::for_each_(const ConditionalChildVisitor &visitor) {
  std::uint32_t i = 0;
  while (true) {
    if (!visitor(*this, i)) {
      break;
    }
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
