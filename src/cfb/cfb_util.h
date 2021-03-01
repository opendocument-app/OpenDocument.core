#ifndef ODR_CFB_UTIL_H
#define ODR_CFB_UTIL_H

#include <abstract/file.h>
#include <cfb/cfb_impl.h>
#include <istream>
#include <memory>
#include <string>

namespace odr::common {
class MemoryFile;
class DiscFile;
} // namespace odr::common

namespace odr::cfb::impl {
class CompoundFileReader;
struct CompoundFileEntry;
} // namespace odr::cfb::impl

namespace odr::cfb::util {

class Archive final {
public:
  explicit Archive(const std::shared_ptr<common::MemoryFile> &file);

  [[nodiscard]] const impl::CompoundFileReader &cfb() const;

  [[nodiscard]] std::shared_ptr<abstract::File> file() const;

private:
  impl::CompoundFileReader m_cfb;
  std::shared_ptr<abstract::File> m_file;
};

class FileInCfb final : public abstract::File {
public:
  FileInCfb(std::shared_ptr<Archive> archive,
            const impl::CompoundFileEntry &entry);

  [[nodiscard]] FileLocation location() const noexcept final;
  [[nodiscard]] std::size_t size() const final;
  [[nodiscard]] std::unique_ptr<std::istream> read() const final;

private:
  std::shared_ptr<Archive> m_archive;
  const impl::CompoundFileEntry &m_entry;
};

} // namespace odr::cfb::util

#endif // ODR_CFB_UTIL_H
