#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>

#include <pugixml.hpp>

namespace odr::internal::xml {

/// The nodes one element owns: a run spans several, everything else is one.
struct NodeSpan final {
  pugi::xml_node first;
  pugi::xml_node last;
};

/// Removes @p span and everything between its ends from the tree.
inline void remove_nodes(const NodeSpan span) {
  pugi::xml_node parent = span.first.parent();
  // the loop invalidates `last`, so where it ends is read first
  const pugi::xml_node end = span.last.next_sibling();
  for (pugi::xml_node node = span.first; node != end;) {
    const pugi::xml_node next = node.next_sibling();
    parent.remove_child(node);
    node = next;
  }
}

/// Splices the element tree of a registry whose elements carry a
/// `pugi::xml_node`, keeping the dom and the registry links in step. Every
/// engine holding its dom does this the same way, so nothing here names a tag.
template <typename Registry> class TreeEditor final {
public:
  explicit TreeEditor(Registry &registry) : m_registry{&registry} {}

  /// The nodes @p element_id owns: a run ends where its `Text` payload says,
  /// everything else is its node and the subtree under it.
  [[nodiscard]] NodeSpan node_span(const ElementIdentifier element_id) const {
    const auto &element = m_registry->element_at(element_id);
    if (element.type == ElementType::text) {
      return {element.node, m_registry->text_element_at(element_id).last};
    }
    return {element.node, element.node};
  }

  /// Removes @p element_id and its subtree; it keeps its id and stops being
  /// reachable.
  void remove(const ElementIdentifier element_id) const {
    remove_nodes(node_span(element_id));
    m_registry->unlink_child(element_id);
  }

private:
  Registry *m_registry{nullptr};
};

} // namespace odr::internal::xml
