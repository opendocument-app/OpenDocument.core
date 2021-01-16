#ifndef ODR_CFB_ARCHIVE_H
#define ODR_CFB_ARCHIVE_H

#include <common/archive.h>

namespace odr::cfb {
class CfbFile;

class CfbArchive final : public common::DefaultArchive {
public:
  CfbArchive();
  explicit CfbArchive(const std::shared_ptr<const CfbFile> &file);

  std::unique_ptr<abstract::ArchiveEntryIterator>
  insert_file(std::unique_ptr<abstract::ArchiveEntryIterator> at,
              common::Path path, std::shared_ptr<abstract::File> file) final;

  void save(std::ostream &) const final;
};

} // namespace odr::cfb

#endif // ODR_CFB_ARCHIVE_H
