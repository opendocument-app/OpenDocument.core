#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace odr::internal {

/// The tree links every registry element carries, stored @p Id wide.
template <typename Id> struct ElementNode {
  Id parent_id{null_element_id};
  Id first_child_id{null_element_id};
  Id last_child_id{null_element_id};
  Id previous_sibling_id{null_element_id};
  Id next_sibling_id{null_element_id};
  ElementType type{ElementType::none};
};

/// A per-type payload keyed by element id, for payloads written out of parse
/// order. @ref SortedSideTable is cheaper where they are not.
template <typename T> class SideTable final {
public:
  T &emplace(const ElementIdentifier id, T value) {
    return m_entries.insert_or_assign(id, std::move(value)).first->second;
  }

  [[nodiscard]] T *find(const ElementIdentifier id) {
    return const_cast<T *>(std::as_const(*this).find(id));
  }

  [[nodiscard]] const T *find(const ElementIdentifier id) const {
    const auto it = m_entries.find(id);
    return it != std::end(m_entries) ? &it->second : nullptr;
  }

  [[nodiscard]] T &at(const ElementIdentifier id) {
    return const_cast<T &>(std::as_const(*this).at(id));
  }

  [[nodiscard]] const T &at(const ElementIdentifier id) const {
    const T *entry = find(id);
    if (entry == nullptr) {
      throw std::out_of_range("SideTable::at: identifier not found");
    }
    return *entry;
  }

private:
  std::unordered_map<ElementIdentifier, T> m_entries;
};

/// A per-type payload keyed by element id and written in id order, so a lookup
/// is a binary search. A deque: `emplace` hands out a reference.
template <typename T, typename Id = ElementIdentifier>
class SortedSideTable final {
public:
  T &emplace(const ElementIdentifier id, T value) {
    if (!m_entries.empty() && m_entries.back().first >= id) {
      throw std::invalid_argument(
          "SortedSideTable::emplace: identifier out of order");
    }
    return m_entries.emplace_back(static_cast<Id>(id), std::move(value)).second;
  }

  [[nodiscard]] T *find(const ElementIdentifier id) {
    return const_cast<T *>(std::as_const(*this).find(id));
  }

  [[nodiscard]] const T *find(const ElementIdentifier id) const {
    const auto it = std::ranges::lower_bound(m_entries, id, {}, &Entry::first);
    return it != std::end(m_entries) && it->first == id ? &it->second : nullptr;
  }

  [[nodiscard]] T &at(const ElementIdentifier id) {
    return const_cast<T &>(std::as_const(*this).at(id));
  }

  [[nodiscard]] const T &at(const ElementIdentifier id) const {
    const T *entry = find(id);
    if (entry == nullptr) {
      throw std::out_of_range("SortedSideTable::at: identifier not found");
    }
    return *entry;
  }

  [[nodiscard]] auto begin() const noexcept { return m_entries.begin(); }
  [[nodiscard]] auto end() const noexcept { return m_entries.end(); }
  [[nodiscard]] auto begin() noexcept { return m_entries.begin(); }
  [[nodiscard]] auto end() noexcept { return m_entries.end(); }

private:
  using Entry = std::pair<Id, T>;

  std::deque<Entry> m_entries;
};

/// The flat store an engine builds its element tree in: an id is the index
/// plus one, and @p ElementT derives from @ref ElementNode for the links.
///
/// A deque by default, because `create_element_` hands back a reference the
/// parser holds on to.
template <typename ElementT, typename Id = ElementIdentifier,
          typename Container = std::deque<ElementT>>
class ElementRegistry {
public:
  using Element = ElementT;

  [[nodiscard]] std::size_t size() const noexcept { return m_elements.size(); }

  [[nodiscard]] Element &element_at(const ElementIdentifier id) {
    check_element_id(id);
    return m_elements[id - 1];
  }

  [[nodiscard]] const Element &element_at(const ElementIdentifier id) const {
    check_element_id(id);
    return m_elements[id - 1];
  }

  void append_child(const ElementIdentifier parent_id,
                    const ElementIdentifier child_id) {
    Element &parent = element_at(parent_id);
    link_child(parent_id, child_id, parent.first_child_id,
               parent.last_child_id);
  }

protected:
  ~ElementRegistry() = default;

  std::tuple<ElementIdentifier, Element &> create_element_(ElementType type) {
    if (m_elements.size() >= std::numeric_limits<Id>::max()) {
      throw std::overflow_error(
          "ElementRegistry::create_element: out of identifiers");
    }

    Element &element = m_elements.emplace_back();
    const ElementIdentifier element_id = m_elements.size();
    element.type = type;
    return {element_id, element};
  }

  /// Links @p child_id as the last child of the chain @p first_id / @p last_id
  /// - the element's own, or one of the secondary chains a payload holds.
  void link_child(const ElementIdentifier parent_id,
                  const ElementIdentifier child_id, Id &first_id, Id &last_id) {
    Element &child = element_at(child_id);
    if (child.parent_id != null_element_id) {
      throw std::invalid_argument(
          "ElementRegistry::link_child: child already has a parent");
    }

    child.parent_id = static_cast<Id>(parent_id);
    child.previous_sibling_id = last_id;

    if (first_id == null_element_id) {
      first_id = static_cast<Id>(child_id);
    } else {
      element_at(last_id).next_sibling_id = static_cast<Id>(child_id);
    }
    last_id = static_cast<Id>(child_id);
  }

  void check_element_id(const ElementIdentifier id) const {
    if (id == null_element_id) {
      throw std::out_of_range(
          "ElementRegistry::check_element_id: null identifier");
    }
    if (id - 1 >= m_elements.size()) {
      throw std::out_of_range(
          "ElementRegistry::check_element_id: identifier out of range");
    }
  }

  Container m_elements;
};

} // namespace odr::internal
