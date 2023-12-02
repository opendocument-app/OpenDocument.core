#ifndef ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H

#include <odr/internal/abstract/document_element.hpp>

#include <pugixml.hpp>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal::common {

struct InternalElementContainer {
  abstract::Element *element;

  ElementIdentifier parent;
  ElementIdentifier first_child;
  ElementIdentifier last_child;
  ElementIdentifier previous_sibling;
  ElementIdentifier next_sibling;
};

class Element : public virtual abstract::Element {
public:
  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  parent(const abstract::Document *, ElementIdentifier) const override;
  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  first_child(const abstract::Document *, ElementIdentifier) const override;
  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  last_child(const abstract::Document *, ElementIdentifier) const override;
  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  previous_sibling(const abstract::Document *,
                   ElementIdentifier) const override;
  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  next_sibling(const abstract::Document *, ElementIdentifier) const override;

  void init_append_child(Element *element);

  Element *m_parent{};
  Element *m_first_child{};
  Element *m_last_child{};
  Element *m_next_sibling{};
  Element *m_previous_sibling{};
};

class Table : public virtual Element, public abstract::Table {
public:
  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  first_child(const abstract::Document *, ElementIdentifier) const final;
  [[nodiscard]] std::pair<abstract::Element *, ElementIdentifier>
  last_child(const abstract::Document *, ElementIdentifier) const final;

  std::pair<abstract::Element *, ElementIdentifier>
  first_column(const abstract::Document *, ElementIdentifier) const final;
  std::pair<abstract::Element *, ElementIdentifier>
  first_row(const abstract::Document *, ElementIdentifier) const final;

  void init_append_column(Element *element);
  void init_append_row(Element *element);

  Element *m_first_column{};
  Element *m_last_column{};
};

class Sheet : public virtual Element, public abstract::Sheet {
public:
  void init_child(Element *element);
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
