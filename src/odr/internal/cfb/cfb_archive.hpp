#pragma once

#include <odr/internal/abstract/archive.hpp>

#include <memory>
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

  [[nodiscard]] std::shared_ptr<abstract::Filesystem>
  as_filesystem() const override;

  void save(std::ostream &out) const override;

private:
  std::shared_ptr<util::Archive> m_cfb;
};

} // namespace odr::internal::cfb
