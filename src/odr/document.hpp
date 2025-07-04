#pragma once

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
class Filesystem;

/// @brief Represents a document.
class Document final {
public:
  explicit Document(std::shared_ptr<internal::abstract::Document>);

  [[nodiscard]] bool is_editable() const noexcept;
  [[nodiscard]] bool is_savable(bool encrypted = false) const noexcept;

  void save(const std::string &path) const;
  void save(const std::string &path, const std::string &password) const;

  [[nodiscard]] FileType file_type() const noexcept;
  [[nodiscard]] DocumentType document_type() const noexcept;

  [[nodiscard]] Element root_element() const;

  [[nodiscard]] Filesystem as_filesystem() const;

private:
  std::shared_ptr<internal::abstract::Document> m_impl;

  friend DocumentFile;
};

} // namespace odr
