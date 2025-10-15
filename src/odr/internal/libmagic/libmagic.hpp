#pragma once

#include <string>

namespace odr::internal::libmagic {
const char *mime_type(const std::string &path);
}
