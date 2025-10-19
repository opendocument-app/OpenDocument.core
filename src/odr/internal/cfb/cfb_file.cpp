#include <odr/internal/cfb/cfb_file.hpp>

#include <odr/internal/cfb/cfb_archive.hpp>
#include <odr/internal/cfb/cfb_util.hpp>

namespace odr::internal::cfb {

CfbFile::CfbFile(const std::shared_ptr<MemoryFile> &file)
    : m_cfb{std::make_shared<util::Archive>(file)} {}

std::shared_ptr<abstract::File> CfbFile::file() const noexcept {
  return m_cfb->file();
}

DecoderEngine CfbFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

FileType CfbFile::file_type() const noexcept {
  return FileType::compound_file_binary_format;
}

std::string_view CfbFile::mimetype() const noexcept {
  return "application/x-cfb";
}

FileMeta CfbFile::file_meta() const noexcept {
  return {file_type(), mimetype(), false, std::nullopt};
}

bool CfbFile::is_decodable() const noexcept { return true; }

std::shared_ptr<abstract::Archive> CfbFile::archive() const {
  return std::make_shared<CfbArchive>(m_cfb);
}

} // namespace odr::internal::cfb
