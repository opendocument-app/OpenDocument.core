#pragma once

#include <odr/internal/abstract/document_element.hpp>

#include <stdexcept>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal {

class Element : public virtual abstract::Element {
public:
  [[nodiscard]] abstract::Element *
  parent(const abstract::Document *) const override;
  [[nodiscard]] abstract::Element *
  first_child(const abstract::Document *) const override;
  [[nodiscard]] abstract::Element *
  last_child(const abstract::Document *) const override;
  [[nodiscard]] abstract::Element *
  previous_sibling(const abstract::Document *) const override;
  [[nodiscard]] abstract::Element *
  next_sibling(const abstract::Document *) const override;

  void append_child_(Element *element);

  template <typename document_t>
  static const document_t &document_(const abstract::Document *document) {
    const auto *cast = dynamic_cast<const document_t *>(document);
    if (cast == nullptr) {
      throw std::runtime_error("unknown document type");
    }
    return *cast;
  }

  Element *m_parent{};
  Element *m_first_child{};
  Element *m_last_child{};
  Element *m_previous_sibling{};
  Element *m_next_sibling{};
};

class Table : public virtual Element, public abstract::Table {
public:
  [[nodiscard]] abstract::Element *
  first_column(const abstract::Document *) const final;
  [[nodiscard]] abstract::Element *
  first_row(const abstract::Document *) const final;

  void append_column_(Element *element);
  void append_row_(Element *element);

  Element *m_first_column{};
  Element *m_last_column{};
};

} // namespace odr::internal
