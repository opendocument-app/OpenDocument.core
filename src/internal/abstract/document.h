#ifndef ODR_INTERNAL_ABSTRACT_DOCUMENT_H
#define ODR_INTERNAL_ABSTRACT_DOCUMENT_H

#include <any>
#include <memory>
#include <string>
#include <unordered_map>

namespace odr {
enum class DocumentType;
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

class DocumentCursor {
public:
  virtual ~DocumentCursor() = default;

  virtual bool operator==(const DocumentCursor &rhs) const = 0;
  virtual bool operator!=(const DocumentCursor &rhs) const = 0;

  [[nodiscard]] virtual std::unique_ptr<DocumentCursor> copy() const = 0;

  [[nodiscard]] virtual std::string document_path() const = 0;

  [[nodiscard]] virtual ElementType type() const = 0;

  virtual bool parent() = 0;
  virtual bool first_child() = 0;
  virtual bool previous_sibling() = 0;
  virtual bool next_sibling() = 0;

  [[nodiscard]] virtual std::unordered_map<ElementProperty, std::any>
  properties() const = 0;
};

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

  /// \return cursor to the root element of the document.
  [[nodiscard]] virtual std::unique_ptr<DocumentCursor>
  root_element() const = 0;
};

} // namespace odr::internal::abstract

#endif // ODR_INTERNAL_ABSTRACT_DOCUMENT_H
