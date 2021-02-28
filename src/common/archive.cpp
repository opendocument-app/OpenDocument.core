#include <cfb/cfb_archive.h>
#include <common/archive.h>
#include <zip/zip_archive.h>

namespace odr::common {

// TODO do we really want to couple these here?
template class Archive<zip::ZipArchive>;
template class Archive<cfb::ReadonlyCfbArchive>;

} // namespace odr::common
