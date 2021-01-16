#include <cfb/cfb_archive.h>
#include <cfb/cfb_file.h>
#include <odr/file.h>

namespace odr::cfb {

CfbFile::CfbFile(std::shared_ptr<common::MemoryFile> file)
    : m_reader{file->content().data(), file->content().size()},
      m_file{std::move(file)} {}

CfbFile::~CfbFile() = default;

FileType CfbFile::file_type() const noexcept { return FileType::ZIP; }

FileCategory CfbFile::file_category() const noexcept {
  return FileCategory::ARCHIVE;
}

FileMeta CfbFile::file_meta() const noexcept {
  return {}; // TODO
}

FileLocation CfbFile::file_location() const noexcept {
  return m_file->file_location();
}

std::size_t CfbFile::size() const { return m_file->size(); }

std::unique_ptr<std::istream> CfbFile::data() const { return m_file->data(); }

impl::CompoundFileReader *CfbFile::impl() const { return &m_reader; }

std::shared_ptr<CfbArchive> CfbFile::archive() const {
  return std::make_shared<CfbArchive>(shared_from_this());
}

} // namespace odr::cfb
