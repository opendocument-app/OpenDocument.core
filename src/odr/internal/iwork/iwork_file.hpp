#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>
#include <string>

namespace odr::internal::abstract {
class Document;
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::iwork {

/// An iWork package, detected from its archives and component list.
class IworkFile final : public abstract::DocumentFile {
public:
  explicit IworkFile(std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] DocumentType document_type() const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  FileMeta m_file_meta;
};

} // namespace odr::internal::iwork
