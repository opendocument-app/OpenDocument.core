#include <common/File.h>
#include <access/Path.h>

namespace odr::common {

bool File::editable() const noexcept { return false; }

bool File::savable(bool) const noexcept { return false; }

void File::save(const access::Path &) const {}

void File::save(const access::Path &, const std::string &) const {}

}
