#include <common/archive.h>
#include <zip/zip_archive.h>
#include <cfb/cfb_archive.h>

namespace odr::common {

// TODO do we really want to couple these here?
template class Archive<zip::ZipArchive>;
template class Archive<cfb::ReadonlyCfbArchive>;

}
