#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT2_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT2_H

#include <any>
#include <memory>
#include <unordered_map>

namespace odr {
enum class DocumentType;
struct DocumentMeta;
enum class ElementType;
enum class ElementProperty;
} // namespace odr

namespace odr::internal::common {
class Path;
}

namespace odr::internal::abstract {
class File;
class ReadableFilesystem;
class Table;

class Element {
public:
  virtual ~Element() = default;

  [[nodiscard]] virtual ElementType type() const = 0;

  [[nodiscard]] virtual Element *parent() const = 0;
  [[nodiscard]] virtual Element *first_child() const = 0;
  [[nodiscard]] virtual Element *previous_sibling() const = 0;
  [[nodiscard]] virtual Element *next_sibling() const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  properties() const = 0;

  virtual void update_properties(
      std::unordered_map<ElementProperty, std::any> properties) const = 0;

  [[nodiscard]] virtual Table *table() const = 0;
};

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
class ElementCursor {
public:
  virtual ~ElementCursor() = default;

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

  [[nodiscard]] virtual Table *table() const = 0;
};

class Document2 {
public:
  virtual ~Document2() = default;

  /// \return `true` if the document is editable in any way.
  [[nodiscard]] virtual bool editable() const noexcept = 0;

  /// \param encrypted to ask for encrypted saves.
  /// \return `true` if the document is savable.
  [[nodiscard]] virtual bool savable(bool encrypted) const noexcept = 0;

  /// \param path the destination path.
  virtual void save(const common::Path &path) const = 0;

  /// \param path the destination path.
  /// \param password the encryption password.
  virtual void save(const common::Path &path, const char *password) const = 0;

  /// \return the type of the document.
  [[nodiscard]] virtual DocumentType document_type() const noexcept = 0;

  [[nodiscard]] virtual std::shared_ptr<ReadableFilesystem>
  files() const noexcept = 0;

  /// \return the root element of the document.
  [[nodiscard]] virtual Element *root_element() const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT2_H
