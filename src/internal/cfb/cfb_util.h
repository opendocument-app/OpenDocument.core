#ifndef ODR_INTERNAL_CFB_UTIL_H
#define ODR_INTERNAL_CFB_UTIL_H

#include <internal/abstract/file.h>
#include <internal/cfb/cfb_impl.h>
#include <istream>
#include <memory>
#include <string>

namespace odr::internal::common {
class MemoryFile;
class DiscFile;
} // namespace odr::internal::common

namespace odr::internal::cfb::impl {
class CompoundFileReader;
struct CompoundFileEntry;
} // namespace odr::internal::cfb::impl

namespace odr::internal::cfb::util {

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

  [[nodiscard]] experimental::FileLocation location() const noexcept final;
  [[nodiscard]] std::size_t size() const final;
  [[nodiscard]] std::unique_ptr<std::istream> read() const final;

private:
  std::shared_ptr<Archive> m_archive;
  const impl::CompoundFileEntry &m_entry;
};

} // namespace odr::internal::cfb::util

#endif // ODR_INTERNAL_CFB_UTIL_H
