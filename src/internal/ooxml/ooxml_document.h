#ifndef ODR_INTERNAL_OOXML_DOCUMENT_H
#define ODR_INTERNAL_OOXML_DOCUMENT_H

#include <internal/abstract/document.h>
#include <memory>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlDocument : public virtual abstract::Document {
public:
  explicit OfficeOpenXmlDocument(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] bool editable() const noexcept final;
  [[nodiscard]] bool savable(bool encrypted) const noexcept final;

  void save(const common::Path &path) const final;
  void save(const common::Path &path, const char *password) const final;

  [[nodiscard]] DocumentType document_type() const noexcept final;

  [[nodiscard]] ElementIdentifier root_element() const final;
  [[nodiscard]] ElementIdentifier first_entry_element() const final;

  [[nodiscard]] ElementType
  element_type(ElementIdentifier element_id) const final;

  [[nodiscard]] ElementIdentifier
  element_parent(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_first_child(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(ElementIdentifier element_id) const final;
  [[nodiscard]] ElementIdentifier
  element_next_sibling(ElementIdentifier element_id) const final;

  [[nodiscard]] const char *
  element_string_property(ElementIdentifier element_id,
                          ElementProperty property) const final;
  [[nodiscard]] std::uint32_t
  element_uint32_property(ElementIdentifier element_id,
                          ElementProperty property) const final;
  [[nodiscard]] bool
  element_bool_property(ElementIdentifier element_id,
                        ElementProperty property) const final;
  [[nodiscard]] const char *
  element_optional_string_property(ElementIdentifier element_id,
                                   ElementProperty property) const final;

  [[nodiscard]] TableDimensions
  table_dimensions(ElementIdentifier element_id, std::uint32_t limit_rows,
                   std::uint32_t limit_cols) const final;

  [[nodiscard]] std::shared_ptr<abstract::File>
  image_file(ElementIdentifier element_id) const final;

  void set_element_string_property(ElementIdentifier element_id,
                                   ElementProperty property,
                                   const char *value) const final;

  void remove_element_property(ElementIdentifier element_id,
                               ElementProperty property) const final;

protected:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
};

} // namespace odr::internal::ooxml

#endif // ODR_INTERNAL_OOXML_DOCUMENT_H
