#ifndef ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
#define ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H

#include <internal/abstract/allocator.h>
#include <internal/abstract/document_element.h>
#include <pugixml.hpp>

namespace odr::internal::abstract {
class Document;
class Element;
} // namespace odr::internal::abstract

namespace odr::internal::common {

template <typename Derived>
abstract::Element *construct(pugi::xml_node node,
                             const abstract::Allocator *allocator) {
  auto alloc = (*allocator)(sizeof(Derived));
  return new (alloc) Derived(node);
}

template <typename Derived, typename... Args>
abstract::Element *construct_2(const abstract::Allocator *allocator,
                               Args &&...args) {
  auto alloc = (*allocator)(sizeof(Derived));
  return new (alloc) Derived(std::forward<Args>(args)...);
}

template <typename Derived>
abstract::Element *construct_optional(pugi::xml_node node,
                                      const abstract::Allocator *allocator) {
  if (!node) {
    return nullptr;
  }
  return construct<Derived>(node, allocator);
}

template <typename ElementConstructor, typename... Args>
abstract::Element *construct_parent_element(ElementConstructor constructor,
                                            pugi::xml_node node,
                                            Args &&...args) {
  for (node = node.parent(); node; node = node.parent()) {
    if (auto result = constructor(node, std::forward<Args>(args)...)) {
      return result;
    }
  }
  return {};
}

template <typename ElementConstructor, typename... Args>
abstract::Element *construct_first_child_element(ElementConstructor constructor,
                                                 pugi::xml_node node,
                                                 Args &&...args) {
  for (node = node.first_child(); node; node = node.next_sibling()) {
    if (auto result = constructor(node, std::forward<Args>(args)...)) {
      return result;
    }
  }
  return {};
}

template <typename ElementConstructor, typename... Args>
abstract::Element *
construct_previous_sibling_element(ElementConstructor constructor,
                                   pugi::xml_node node, Args &&...args) {
  for (node = node.previous_sibling(); node; node = node.previous_sibling()) {
    if (auto result = constructor(node, std::forward<Args>(args)...)) {
      return result;
    }
  }
  return {};
}

template <typename ElementConstructor, typename... Args>
abstract::Element *
construct_next_sibling_element(ElementConstructor constructor,
                               pugi::xml_node node, Args &&...args) {
  for (node = node.next_sibling(); node; node = node.next_sibling()) {
    if (auto result = constructor(node, std::forward<Args>(args)...)) {
      return result;
    }
  }
  return {};
}

template <typename Derived> class Element : public virtual abstract::Element {
public:
  explicit Element(const pugi::xml_node node) : m_node{node} {}

  [[nodiscard]] bool equals(const abstract::Element &rhs) const override {
    return m_node == *dynamic_cast<const Element &>(rhs).m_node;
  }

  abstract::Element *
  construct_parent(const abstract::Document *,
                   const abstract::Allocator *allocator) const override {
    return common::construct_parent_element(Derived::construct_default_element,
                                            m_node, allocator);
  }

  abstract::Element *
  construct_first_child(const abstract::Document *,
                        const abstract::Allocator *allocator) const override {
    return common::construct_first_child_element(
        Derived::construct_default_element, m_node, allocator);
  }

  abstract::Element *construct_previous_sibling(
      const abstract::Document *,
      const abstract::Allocator *allocator) const override {
    return common::construct_previous_sibling_element(
        Derived::construct_default_element, m_node, allocator);
  }

  abstract::Element *
  construct_next_sibling(const abstract::Document *,
                         const abstract::Allocator *allocator) const override {
    return common::construct_next_sibling_element(
        Derived::construct_default_element, m_node, allocator);
  }

  bool move_to_parent(const abstract::Document *) override { return false; }
  bool move_to_first_child(const abstract::Document *) override {
    return false;
  }
  bool move_to_previous_sibling(const abstract::Document *) override {
    return false;
  }
  bool move_to_next_sibling(const abstract::Document *) override {
    return false;
  }

protected:
  pugi::xml_node m_node;
};

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_DOCUMENT_ELEMENT_H
