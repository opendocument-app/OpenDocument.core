#pragma once

#include <odr/definitions.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>

#include <stdexcept>

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

  /// Splits @p element_id after @p after_id - one of its descendants, or null
  /// to move every child - and answers the copy holding what followed. Every
  /// element on the way up is split too, and only a span and a link are ones
  /// it will split through.
  ElementIdentifier split(const ElementIdentifier element_id,
                          const ElementIdentifier after_id) const {
    ElementIdentifier stays_id = after_id;
    ElementIdentifier level_id = element_id;

    if (after_id != null_element_id) {
      level_id = m_registry->element_at(after_id).parent_id;
      if (!is_ancestor_(element_id, level_id)) {
        throw std::invalid_argument(
            "TreeEditor::split: the element to split after is not a "
            "descendant");
      }
      for (ElementIdentifier at_id = level_id; at_id != element_id;
           at_id = m_registry->element_at(at_id).parent_id) {
        const ElementType type = m_registry->element_at(at_id).type;
        if (type != ElementType::span && type != ElementType::link) {
          throw UnsupportedOperation();
        }
      }
    }

    while (level_id != element_id) {
      split_level_(level_id, stays_id);
      stays_id = level_id;
      level_id = m_registry->element_at(level_id).parent_id;
    }
    return split_level_(element_id, stays_id);
  }

  /// Takes the children of @p element_id's next sibling and removes it.
  void merge_next(const ElementIdentifier element_id) const {
    const ElementIdentifier next_id =
        m_registry->element_at(element_id).next_sibling_id;
    if (next_id == null_element_id) {
      throw std::invalid_argument("TreeEditor::merge_next: nothing follows");
    }
    move_children_(next_id, null_element_id, element_id);
    remove(next_id);
  }

  /// An empty element of the same kind after @p element_id.
  ElementIdentifier
  insert_sibling_after(const ElementIdentifier element_id) const {
    return clone_shell_after_(element_id);
  }

private:
  Registry *m_registry{nullptr};

  /// A copy of @p element_id right after it, with what the format says about
  /// it and none of its content.
  ElementIdentifier
  clone_shell_after_(const ElementIdentifier element_id) const {
    const pugi::xml_node node = m_registry->element_at(element_id).node;
    const ElementType type = m_registry->element_at(element_id).type;

    pugi::xml_node copy = node.parent().insert_child_after(
        node.name(), node_span(element_id).last);
    for (const pugi::xml_attribute attribute : node.attributes()) {
      copy.append_copy(attribute);
    }
    // `w:pPr`, `w:rPr` and their like sit ahead of the first child the
    // registry knows, which is where the content starts and they end
    const pugi::xml_node content = first_child_node_(element_id);
    for (pugi::xml_node child = node.first_child(); child && child != content;
         child = child.next_sibling()) {
      copy.append_copy(child);
    }

    const auto &[copy_id, unused] = m_registry->create_element(type, copy);
    m_registry->insert_sibling_after(element_id, copy_id);
    return copy_id;
  }

  /// Copies @p element_id's shell and moves what follows @p after_id into it.
  ElementIdentifier split_level_(const ElementIdentifier element_id,
                                 const ElementIdentifier after_id) const {
    const ElementIdentifier copy_id = clone_shell_after_(element_id);
    move_children_(element_id, after_id, copy_id);
    return copy_id;
  }

  /// Moves the children of @p element_id after @p after_id - all of them where
  /// that is null - to the end of @p target_id.
  void move_children_(const ElementIdentifier element_id,
                      const ElementIdentifier after_id,
                      const ElementIdentifier target_id) const {
    ElementIdentifier child_id =
        after_id == null_element_id
            ? m_registry->element_at(element_id).first_child_id
            : m_registry->element_at(after_id).next_sibling_id;
    while (child_id != null_element_id) {
      const ElementIdentifier next_id =
          m_registry->element_at(child_id).next_sibling_id;
      move_child_(child_id, target_id);
      child_id = next_id;
    }
  }

  void move_child_(const ElementIdentifier child_id,
                   const ElementIdentifier target_id) const {
    const NodeSpan span = node_span(child_id);
    pugi::xml_node target = m_registry->element_at(target_id).node;
    // the moves invalidate `next_sibling`, so where the span ends is read
    // first and each step reads its own successor before it moves
    const pugi::xml_node end = span.last.next_sibling();
    for (pugi::xml_node node = span.first; node != end;) {
      const pugi::xml_node next = node.next_sibling();
      target.append_move(node);
      node = next;
    }

    m_registry->unlink_child(child_id);
    m_registry->append_child(target_id, child_id);
  }

  /// Whether @p element_id is @p at_id or one of the elements above it.
  [[nodiscard]] bool is_ancestor_(const ElementIdentifier element_id,
                                  const ElementIdentifier at_id) const {
    for (ElementIdentifier walk_id = at_id; walk_id != null_element_id;
         walk_id = m_registry->element_at(walk_id).parent_id) {
      if (walk_id == element_id) {
        return true;
      }
    }
    return false;
  }

  /// The node @p element_id's first child starts at, null where it has none.
  [[nodiscard]] pugi::xml_node
  first_child_node_(const ElementIdentifier element_id) const {
    const ElementIdentifier child_id =
        m_registry->element_at(element_id).first_child_id;
    return child_id == null_element_id ? pugi::xml_node()
                                       : node_span(child_id).first;
  }
};

} // namespace odr::internal::xml
