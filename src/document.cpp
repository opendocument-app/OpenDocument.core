#include <internal/abstract/document.h>
#include <internal/common/path.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_meta.h>
#include <odr/document_style.h>

namespace odr {

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

std::uint32_t Document::entry_count() const { return m_impl->entry_count(); }

Element Document::root() const {
  return Element(m_impl, m_impl->root_element());
}

Element Document::first_entry() const {
  return Element(m_impl, m_impl->first_entry_element());
}

TextDocument Document::text_document() const { return TextDocument(m_impl); }

Presentation Document::presentation() const { return Presentation(m_impl); }

Spreadsheet Document::spreadsheet() const { return Spreadsheet(m_impl); }

Drawing Document::drawing() const { return Drawing(m_impl); }

TextDocument::TextDocument(
    std::shared_ptr<internal::abstract::Document> text_document)
    : Document(std::move(text_document)) {
  if (m_impl->document_type() != DocumentType::TEXT) {
    throw std::runtime_error("not a text document");
  }
}

ElementRange TextDocument::content() const { return root().children(); }

PageStyle TextDocument::page_style() const {
  return PageStyle(m_impl, m_impl->root_element());
}

Presentation::Presentation(
    std::shared_ptr<internal::abstract::Document> presentation)
    : Document(std::move(presentation)) {
  if (m_impl->document_type() != DocumentType::PRESENTATION) {
    throw std::runtime_error("not a presentation");
  }
}

std::uint32_t Presentation::slide_count() const {
  return m_impl->entry_count();
}

SlideRange Presentation::slides() const {
  return SlideRange(SlideElement(m_impl, m_impl->first_entry_element()));
}

Spreadsheet::Spreadsheet(
    std::shared_ptr<internal::abstract::Document> spreadsheet)
    : Document(std::move(spreadsheet)) {
  if (m_impl->document_type() != DocumentType::SPREADSHEET) {
    throw std::runtime_error("not a spreadsheet");
  }
}

std::uint32_t Spreadsheet::sheet_count() const { return m_impl->entry_count(); }

SheetRange Spreadsheet::sheets() const {
  return SheetRange(SheetElement(m_impl, m_impl->first_entry_element()));
}

Drawing::Drawing(std::shared_ptr<internal::abstract::Document> drawing)
    : Document(std::move(drawing)) {
  if (m_impl->document_type() != DocumentType::DRAWING) {
    throw std::runtime_error("not a drawing");
  }
}

std::uint32_t Drawing::page_count() const { return m_impl->entry_count(); }

PageRange Drawing::pages() const {
  return PageRange(PageElement(m_impl, m_impl->first_entry_element()));
}

ElementPropertyValue::ElementPropertyValue() = default;

ElementPropertyValue::ElementPropertyValue(
    std::shared_ptr<const internal::abstract::Document> impl,
    const std::uint64_t id, const ElementProperty property)
    : m_impl{std::move(impl)}, m_id{id}, m_property{property} {}

bool ElementPropertyValue::operator==(const ElementPropertyValue &rhs) const {
  return m_impl == rhs.m_impl && m_id == rhs.m_id &&
         m_property == rhs.m_property;
}

bool ElementPropertyValue::operator!=(const ElementPropertyValue &rhs) const {
  return m_impl != rhs.m_impl || m_id != rhs.m_id ||
         m_property != rhs.m_property;
}

ElementPropertyValue::operator bool() const {
  return m_impl.operator bool() && m_id != 0;
}

std::any ElementPropertyValue::get() const {
  return m_impl->element_property(m_id, m_property);
}

std::string ElementPropertyValue::get_string() const {
  return m_impl->element_string_property(m_id, m_property);
}

std::uint32_t ElementPropertyValue::get_uint32() const {
  return m_impl->element_uint32_property(m_id, m_property);
}

bool ElementPropertyValue::get_bool() const {
  return m_impl->element_bool_property(m_id, m_property);
}

const char *ElementPropertyValue::get_optional_string() const {
  return m_impl->element_optional_string_property(m_id, m_property);
}

void ElementPropertyValue::set(const std::any &value) const {
  m_impl->set_element_property(m_id, m_property, value);
}

void ElementPropertyValue::set_string(const std::string &value) const {
  m_impl->set_element_property(m_id, m_property, value.c_str());
}

void ElementPropertyValue::remove() const {
  m_impl->remove_element_property(m_id, m_property);
}

} // namespace odr
