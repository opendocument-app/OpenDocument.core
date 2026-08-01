#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr::internal {

/// Audio and video: nothing is decoded, the bytes are handed to the browser as
/// they are. One class covers both, the category coming from the file type
/// table rather than from the class.
class MediaFile final : public abstract::DecodedFile {
public:
  MediaFile(std::shared_ptr<abstract::File> file, FileType file_type);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] FileCategory file_category() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;

  [[nodiscard]] bool is_decodable() const noexcept override;

private:
  std::shared_ptr<abstract::File> m_file;
  FileType m_file_type;
};

} // namespace odr::internal
