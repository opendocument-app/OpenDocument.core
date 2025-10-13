#include <odr/internal/common/image_file.hpp>

#include <odr/exceptions.hpp>

namespace odr::internal {

ImageFile::ImageFile(std::shared_ptr<abstract::File> file,
                     const FileType file_type)
    : m_file{std::move(file)}, m_file_type{file_type} {}

std::shared_ptr<abstract::File> ImageFile::file() const noexcept {
  return m_file;
}

FileType ImageFile::file_type() const noexcept { return m_file_type; }

FileMeta ImageFile::file_meta() const noexcept { return {}; }

DecoderEngine ImageFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

bool ImageFile::is_decodable() const noexcept { return false; }

std::shared_ptr<abstract::Image> ImageFile::image() const {
  throw UnsupportedFileEncoding("generally unsupported");
}

} // namespace odr::internal
