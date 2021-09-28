#ifndef ODR_INTERNAL_COMMON_ELEMENT_H
#define ODR_INTERNAL_COMMON_ELEMENT_H

#include <pugixml.hpp>

namespace odr::internal::abstract {
class Element;
} // namespace odr::internal::abstract

namespace odr::internal::common {

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

} // namespace odr::internal::common

#endif // ODR_INTERNAL_COMMON_ELEMENT_H
