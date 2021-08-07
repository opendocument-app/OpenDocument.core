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

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  element_properties(ElementIdentifier element_id) const override;

  void update_element_properties(
      ElementIdentifier element_id,
      std::unordered_map<ElementProperty, std::any> properties) const override;

  [[nodiscard]] std::shared_ptr<abstract::Table>
  table(ElementIdentifier element_id) const override;

protected:
  class PropertyAdapter {
  public:
    virtual ~PropertyAdapter() = default;

    [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
    properties(ElementIdentifier element_id) const = 0;

    virtual void update_properties(
        ElementIdentifier element_id,
        std::unordered_map<ElementProperty, std::any> properties) const = 0;
  };

  struct Element final {
    ElementType type{ElementType::NONE};
    PropertyAdapter *adapter{nullptr};

    ElementIdentifier parent;
    ElementIdentifier first_child;
    ElementIdentifier previous_sibling;
    ElementIdentifier next_sibling;
  };

  DenseElementAttributeStore<Element> m_elements;
  ElementIdentifier m_root;

  SparseElementAttributeStore<std::shared_ptr<abstract::Table>> m_tables;

  ElementIdentifier new_element_(ElementType type, PropertyAdapter *adapter,
                                 ElementIdentifier parent,
                                 ElementIdentifier previous_sibling);
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_H
