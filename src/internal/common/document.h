#ifndef ODR_INTERNAL_COMMON_DOCUMENT_H
#define ODR_INTERNAL_COMMON_DOCUMENT_H

#include <internal/abstract/document.h>
#include <internal/common/element_attributes_store.h>
#include <odr/document.h>

namespace odr::abstract {
class Table;
}

namespace odr::internal::common {

class Document : public abstract::Document {
public:
  [[nodiscard]] ElementIdentifier root_element() const override;
  [[nodiscard]] ElementIdentifier first_entry_element() const override;

  [[nodiscard]] ElementType
  element_type(ElementIdentifier element_id) const override;

  [[nodiscard]] ElementIdentifier
  element_parent(ElementIdentifier element_id) const override;
  [[nodiscard]] ElementIdentifier
  element_first_child(ElementIdentifier element_id) const override;
  [[nodiscard]] ElementIdentifier
  element_previous_sibling(ElementIdentifier element_id) const override;
  [[nodiscard]] ElementIdentifier
  element_next_sibling(ElementIdentifier element_id) const override;

  [[nodiscard]] std::shared_ptr<abstract::Table>
  table(ElementIdentifier element_id) const override;

protected:
  struct Element final {
    ElementType type{ElementType::NONE};

    ElementIdentifier parent;
    ElementIdentifier first_child;
    ElementIdentifier previous_sibling;
    ElementIdentifier next_sibling;
  };

  struct TableElementExtension final {
    std::uint32_t row;
    std::uint32_t column;
  };

  DenseElementAttributeStore<Element> m_elements;
  ElementIdentifier m_root;

  SparseElementAttributeStore<std::shared_ptr<abstract::Table>> m_tables;
  SparseElementAttributeStore<TableElementExtension> m_table_element_extension;

  ElementIdentifier new_element_(ElementType type, ElementIdentifier parent,
                                 ElementIdentifier previous_sibling);
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_H
