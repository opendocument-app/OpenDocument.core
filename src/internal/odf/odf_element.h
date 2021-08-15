#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <any>
#include <memory>
#include <odr/document.h>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr {
enum class ElementType;
enum class ElementProperty;
} // namespace odr

namespace odr::internal::odf {

class ElementAdapter;

class Element;
class TableElement;
class TableColumnElement;
class TableRowElement;
class TableCellElement;

using ElementRange = ElementRangeTemplate<Element>;
using TableColumnElementRange = ElementRangeTemplate<TableColumnElement>;
using TableRowElementRange = ElementRangeTemplate<TableRowElement>;
using TableCellElementRange = ElementRangeTemplate<TableCellElement>;

class Element final {
public:
  Element();
  explicit Element(pugi::xml_node node);
  Element(pugi::xml_node node, ElementAdapter *adapter);

  bool operator==(const Element &rhs) const;
  bool operator!=(const Element &rhs) const;
  explicit operator bool() const;

  [[nodiscard]] pugi::xml_node xml_node() const;
  [[nodiscard]] ElementAdapter *adapter() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TableElement table() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  [[nodiscard]] ElementRange children() const;

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const;

  void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const;

private:
  pugi::xml_node m_node;
  ElementAdapter *m_adapter{nullptr};
};

class ElementAdapter {
public:
  static ElementAdapter *default_adapter(pugi::xml_node node);

  virtual ~ElementAdapter() = default;

  [[nodiscard]] virtual ElementType type(pugi::xml_node node) const = 0;

  [[nodiscard]] virtual Element parent(pugi::xml_node node) const = 0;
  [[nodiscard]] virtual Element first_child(pugi::xml_node node) const = 0;
  [[nodiscard]] virtual Element previous_sibling(pugi::xml_node node) const = 0;
  [[nodiscard]] virtual Element next_sibling(pugi::xml_node node) const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  properties(pugi::xml_node node) const = 0;

  virtual void update_properties(
      pugi::xml_node node,
      std::unordered_map<ElementProperty, std::any> properties) const = 0;
};

template <typename Derived> class ElementBase {
public:
  using Base = ElementBase<Derived>;

  ElementBase() = default;
  explicit ElementBase(pugi::xml_node node) : m_node{std::move(node)} {}

  bool operator==(const Derived &rhs) const { return m_node == rhs.m_node; }
  bool operator!=(const Derived &rhs) const { return m_node != rhs.m_node; }
  explicit operator bool() const { return m_node; }

  [[nodiscard]] pugi::xml_node xml_node() const { return m_node; }

  [[nodiscard]] Element parent() const {
    return Derived::adapter()->parent(m_node);
  }

  [[nodiscard]] Element first_child() const {
    return Derived::adapter()->first_child(m_node);
  }

  [[nodiscard]] Element previous_sibling() const {
    return Derived::adapter()->previous_sibling(m_node);
  }

  [[nodiscard]] Element next_sibling() const {
    return Derived::adapter()->next_sibling(m_node);
  }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const {
    return Derived::adapter()->properties(m_node);
  }

  void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const {
    return Derived::adapter()->update_properties(m_node, std::move(properties));
  }

protected:
  pugi::xml_node m_node;
};

class TableElement final : public ElementBase<TableElement> {
public:
  static ElementAdapter *adapter();

  TableElement();
  explicit TableElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  TableColumnElementRange columns() const;
  TableRowElementRange rows() const;
};

class TableColumnElement final : public ElementBase<TableColumnElement> {
public:
  static ElementAdapter *adapter();

  TableColumnElement();
  explicit TableColumnElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TableElement parent() const;
  [[nodiscard]] TableColumnElement previous_sibling() const;
  [[nodiscard]] TableColumnElement next_sibling() const;
};

class TableRowElement final : public ElementBase<TableRowElement> {
public:
  static ElementAdapter *adapter();

  TableRowElement();
  explicit TableRowElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TableElement parent() const;
  [[nodiscard]] TableRowElement previous_sibling() const;
  [[nodiscard]] TableRowElement next_sibling() const;

  TableCellElementRange cells() const;
};

class TableCellElement final : public ElementBase<TableCellElement> {
public:
  static ElementAdapter *adapter();

  TableCellElement();
  explicit TableCellElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TableRowElement parent() const;
  [[nodiscard]] TableCellElement previous_sibling() const;
  [[nodiscard]] TableCellElement next_sibling() const;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
