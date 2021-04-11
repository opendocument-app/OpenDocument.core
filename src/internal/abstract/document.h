#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_H

#include <any>
#include <cstdint>
#include <internal/identifier.h>
#include <memory>
#include <optional>
#include <string>
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

class Document {
public:
  virtual ~Document() = default;

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
  [[nodiscard]] virtual ElementIdentifier root_element() const = 0;

  /// \return the first entry element of the document.
  [[nodiscard]] virtual ElementIdentifier first_entry_element() const = 0;

  /// \param element_id the element to query.
  /// \return the type of the element.
  [[nodiscard]] virtual ElementType
  element_type(ElementIdentifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the parent of the element.
  [[nodiscard]] virtual ElementIdentifier
  element_parent(ElementIdentifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the first child of the element.
  [[nodiscard]] virtual ElementIdentifier
  element_first_child(ElementIdentifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the previous sibling of the element.
  [[nodiscard]] virtual ElementIdentifier
  element_previous_sibling(ElementIdentifier element_id) const = 0;

  /// \param element_id the element to query.
  /// \return the next sibling of the element.
  [[nodiscard]] virtual ElementIdentifier
  element_next_sibling(ElementIdentifier element_id) const = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  element_properties(ElementIdentifier element_id) const = 0;

  virtual void update_element_properties(
      ElementIdentifier element_id,
      std::unordered_map<ElementProperty, std::any> properties) const = 0;

  [[nodiscard]] virtual std::shared_ptr<Table>
  table(ElementIdentifier element_id) const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
