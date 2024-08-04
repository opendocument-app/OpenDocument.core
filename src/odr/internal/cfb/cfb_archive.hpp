#ifndef ODR_INTERNAL_CFB_ARCHIVE_HPP
#define ODR_INTERNAL_CFB_ARCHIVE_HPP

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace odr {
enum class FileType;
struct FileMeta;
} // namespace odr

namespace odr::internal::abstract {
class File;
} // namespace odr::internal::abstract

namespace odr::internal::cfb::impl {
struct CompoundFileEntry;
} // namespace odr::internal::cfb::impl

namespace odr::internal::cfb::util {
class Archive;
}

namespace odr::internal::cfb {

class CfbArchive final : public abstract::Archive {
public:
  explicit CfbArchive(std::shared_ptr<util::Archive> archive);

  [[nodiscard]] std::shared_ptr<abstract::Filesystem> filesystem() const final;

  void save(std::ostream &out) const final;

private:
  std::shared_ptr<util::Archive> m_cfb;
};

} // namespace odr::internal::cfb

#endif // ODR_INTERNAL_CFB_ARCHIVE_HPP
