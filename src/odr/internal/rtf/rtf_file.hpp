#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>
#include <string_view>

namespace odr::internal::rtf {

class RtfFile final : public abstract::DocumentFile {
public:
  /// @throws NoRtfFile if the bytes do not open with `{\rtf1`.
  explicit RtfFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] DocumentType document_type() const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal::rtf
