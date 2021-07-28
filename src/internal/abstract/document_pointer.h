#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_POINTER_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_POINTER_H

#include <any>
#include <memory>
#include <unordered_map>

namespace odr::internal::abstract {

/*
 * TODO unused - remove?
 *
 * advantages
 *  - this interface is more natural than the existing `abstract::Document`
 * interface to access elements
 *  - supports partially loaded document trees
 *
 * disadvantages
 *  - the current frontend would cause memory allocations by traversing the
 * document tree
 */
class DocumentPointer {
public:
  virtual ~DocumentPointer() = default;

  [[nodiscard]] virtual explicit operator bool() const noexcept = 0;

  [[nodiscard]] virtual std::unique_ptr<DocumentPointer> clone() const = 0;

  [[nodiscard]] virtual ElementType type() const = 0;

  virtual void parent() = 0;
  virtual void first_child() = 0;
  virtual void previous_sibling() = 0;
  virtual void next_sibling() = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  properties() const = 0;

  virtual void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const = 0;

  [[nodiscard]] virtual std::shared_ptr<Table> table() const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_POINTER_H
