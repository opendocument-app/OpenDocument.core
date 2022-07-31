#ifndef ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H

#include <odr/internal/abstract/document_element.hpp>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal::common {

class Element : public virtual abstract::Element {
public:
  explicit Element(const pugi::xml_node node);

  [[nodiscard]] Element *parent(const abstract::Document *) const override;
  [[nodiscard]] Element *first_child(const abstract::Document *) const override;
  [[nodiscard]] Element *last_child(const abstract::Document *) const override;
  [[nodiscard]] Element *
  previous_sibling(const abstract::Document *) const override;
  [[nodiscard]] Element *
  next_sibling(const abstract::Document *) const override;

  void init_append_child(Element *element);

  // TODO protect members
public:
  pugi::xml_node m_node;

  Element *m_parent{};
  Element *m_first_child{};
  Element *m_last_child{};
  Element *m_previous_sibling{};
  Element *m_next_sibling{};
};

class Table : public virtual Element, public abstract::Table {
public:
  explicit Table(const pugi::xml_node node);

  [[nodiscard]] Element *first_child(const abstract::Document *) const final;
  [[nodiscard]] Element *last_child(const abstract::Document *) const final;

  Element *first_column(const abstract::Document *document) const final;
  Element *first_row(const abstract::Document *document) const final;

  void init_append_column(Element *element);
  void init_append_row(Element *element);

public:
  Element *m_first_column{};
  Element *m_last_column{};
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
