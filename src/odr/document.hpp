#pragma once

#include <iosfwd>
#include <memory>
#include <string>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr {
enum class FileType;
enum class DocumentType;
class DocumentFile;
class Element;
class File;
class Filesystem;

/// @brief Represents a document.
class Document final {
public:
  explicit Document(std::shared_ptr<internal::abstract::Document>);

  [[nodiscard]] bool is_editable() const noexcept;
  /// Savable, @p encrypted to ask for an encrypted save. False for a document
  /// decrypted from a password-protected package: saving one can only write it
  /// out in the clear.
  [[nodiscard]] bool is_savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  void save(std::ostream &out) const;
  void save(std::ostream &out, const std::string &password) const;

  /// @brief The saved document as a file in memory.
  [[nodiscard]] File save_to_memory() const;
  [[nodiscard]] File save_to_memory(const std::string &password) const;

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] DocumentType document_type() const noexcept;

  [[nodiscard]] Element root_element() const;

  /// The files the document is packaged from; empty for a document that is
  /// one file.
  [[nodiscard]] Filesystem as_filesystem() const;

private:
  std::shared_ptr<internal::abstract::Document> m_impl;

  friend DocumentFile;
};

} // namespace odr
