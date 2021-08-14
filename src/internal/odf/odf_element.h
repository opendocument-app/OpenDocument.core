#ifndef ODR_INTERNAL_ODF_ELEMENT_H
#define ODR_INTERNAL_ODF_ELEMENT_H

#include <any>
#include <memory>
#include <pugixml.hpp>
#include <unordered_map>

namespace odr {
enum class ElementType;
enum class ElementProperty;
} // namespace odr

namespace odr::internal::odf {

class ElementAdapter;

class TableElement;
class TableColumnElement;
class TableRowElement;
class TableCellElement;

class ElementIterator;

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

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element first_child() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  ElementIterator begin() const;
  ElementIterator end() const;

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
  static std::shared_ptr<ElementAdapter> table_adapter();
  static std::shared_ptr<ElementAdapter> table_column_adapter();
  static std::shared_ptr<ElementAdapter> table_row_adapter();
  static std::shared_ptr<ElementAdapter> table_cell_adapter();
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
  ElementBase() = default;
  explicit ElementBase(pugi::xml_node node) : m_node{std::move(node)} {}

  bool operator==(const Derived &rhs) const { return m_node == rhs.m_node; }
  bool operator!=(const Derived &rhs) const { return m_node != rhs.m_node; }
  explicit operator bool() const { return m_node; }

  [[nodiscard]] pugi::xml_node xml_node() const { return m_node; }

  [[nodiscard]] std::unordered_map<ElementProperty, std::any>
  properties() const {
    return Derived::adapter_()->properties();
  }

  void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const {
    return Derived::adapter_()->update_properties(std::move(properties));
  }

protected:
  pugi::xml_node m_node;
};

class TableElement final : public ElementBase<TableElement> {
public:
  TableElement();
  explicit TableElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] Element parent() const;
  [[nodiscard]] Element previous_sibling() const;
  [[nodiscard]] Element next_sibling() const;

  void columns();
  void rows();

private:
  static std::shared_ptr<ElementAdapter> adapter_();
};

class TableColumnElement final : public ElementBase<TableColumnElement> {
public:
  TableColumnElement();
  explicit TableColumnElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TableElement parent() const;
  [[nodiscard]] TableColumnElement previous_sibling() const;
  [[nodiscard]] TableColumnElement next_sibling() const;

private:
  static std::shared_ptr<ElementAdapter> adapter_();
};

class TableRowElement final : public ElementBase<TableRowElement> {
public:
  TableRowElement();
  explicit TableRowElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TableElement parent() const;
  [[nodiscard]] TableRowElement previous_sibling() const;
  [[nodiscard]] TableRowElement next_sibling() const;

private:
  static std::shared_ptr<ElementAdapter> adapter_();
};

class TableCellElement final : public ElementBase<TableCellElement> {
public:
  TableCellElement();
  explicit TableCellElement(pugi::xml_node node);

  Element element() const;

  [[nodiscard]] ElementType type() const;

  [[nodiscard]] TableRowElement parent() const;
  [[nodiscard]] TableCellElement previous_sibling() const;
  [[nodiscard]] TableCellElement next_sibling() const;

private:
  static std::shared_ptr<ElementAdapter> adapter_();
};

class ElementIterator final {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = Element;
  using difference_type = std::ptrdiff_t;
  using pointer = Element *;
  using reference = Element &;

  explicit ElementIterator(Element element);

  ElementIterator &operator++();
  ElementIterator operator++(int) &;
  reference operator*();
  pointer operator->();
  bool operator==(const ElementIterator &rhs) const;
  bool operator!=(const ElementIterator &rhs) const;

private:
  Element m_element;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_ELEMENT_H
