#pragma once

#include <memory>

namespace odr {
class File;
enum class FileType;
enum class DocumentType;
} // namespace odr

namespace odr::internal {
class Path;
} // namespace odr::internal

namespace odr::internal::abstract {
class ReadableFilesystem;
class ElementAdapter;

class Document {
public:
  virtual ~Document() = default;

  /// \return `true` if the document is editable in any way.
  [[nodiscard]] virtual bool is_editable() const noexcept = 0;

  /// \param encrypted to ask for encrypted saves.
  /// \return `true` if the document is is_savable.
  [[nodiscard]] virtual bool is_savable(bool encrypted) const noexcept = 0;

  /// \param path the destination path.
  virtual void save(const Path &path) const = 0;

  /// \param path the destination path.
  /// \param password the encryption password.
  virtual void save(const Path &path, const char *password) const = 0;

  /// \return the type of the document.
  [[nodiscard]] virtual FileType file_type() const noexcept = 0;

  /// \return the type of the document.
  [[nodiscard]] virtual DocumentType document_type() const noexcept = 0;

  /// \return the underlying filesystem of the document.
  [[nodiscard]] virtual std::shared_ptr<ReadableFilesystem>
  as_filesystem() const noexcept = 0;

  /// \return cursor to the root element of the document.
  [[nodiscard]] virtual ElementIdentifier root_element() const = 0;

  [[nodiscard]] virtual const ElementAdapter *element_adapter() const = 0;
};

} // namespace odr::internal::abstract
