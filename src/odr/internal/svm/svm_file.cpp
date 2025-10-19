#include <odr/exceptions.hpp>

#include <odr/internal/svm/svm_file.hpp>

#include <odr/internal/svm/svm_format.hpp>

#include <utility>

namespace odr::internal::abstract {
class Image;
} // namespace odr::internal::abstract

namespace odr::internal::svm {

SvmFile::SvmFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  const auto in = m_file->stream();
  read_header(*in);
  // TODO store header?
}

std::shared_ptr<abstract::File> SvmFile::file() const noexcept {
  return m_file;
}

FileType SvmFile::file_type() const noexcept {
  return FileType::starview_metafile;
}

FileMeta SvmFile::file_meta() const noexcept {
  return {FileType::starview_metafile, "application/x-starview-metafile", false,
          std::nullopt};
}

DecoderEngine SvmFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

std::string_view SvmFile::mimetype() const noexcept {
  return "application/x-starview-metafile";
}

bool SvmFile::is_decodable() const noexcept { return false; }

std::shared_ptr<abstract::Image> SvmFile::image() const {
  throw UnsupportedFileEncoding("generally unsupported");
}

} // namespace odr::internal::svm
