#include <internal/svm/svm_file.h>
#include <internal/svm/svm_format.h>
#include <odr/file.h>

namespace odr::internal::svm {

SvmFile::SvmFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  auto in = m_file->read();
  read_header(*in);
  // TODO store header?
}

std::shared_ptr<abstract::File> SvmFile::file() const noexcept {
  return m_file;
}

FileType SvmFile::file_type() const noexcept {
  return FileType::STARVIEW_METAFILE;
}

FileMeta SvmFile::file_meta() const noexcept {
  FileMeta result;
  result.type = FileType::STARVIEW_METAFILE;
  return result;
}

std::shared_ptr<abstract::Image> SvmFile::image() const { return {}; }

} // namespace odr::internal::svm
