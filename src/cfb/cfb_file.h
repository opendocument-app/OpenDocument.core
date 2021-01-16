#ifndef ODR_CFB_FILE_H
#define ODR_CFB_FILE_H

#include <cfb/cfb_impl.h>
#include <common/file.h>

namespace odr::cfb {
class CfbArchive;

class CfbFile final : public abstract::File,
                      public std::enable_shared_from_this<CfbFile> {
public:
  explicit CfbFile(std::shared_ptr<common::MemoryFile> file);
  ~CfbFile() final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileCategory file_category() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] FileLocation file_location() const noexcept final;

  [[nodiscard]] std::size_t size() const final;

  [[nodiscard]] std::unique_ptr<std::istream> data() const final;

  [[nodiscard]] impl::CompoundFileReader *impl() const;

  [[nodiscard]] std::shared_ptr<CfbArchive> archive() const;

private:
  mutable impl::CompoundFileReader m_reader;
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::cfb

#endif // ODR_CFB_FILE_H
