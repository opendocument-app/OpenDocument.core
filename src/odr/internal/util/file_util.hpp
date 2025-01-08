#pragma once

#include <string>

namespace odr::internal::util::file {

std::size_t size(const std::string &path);

std::string read(const std::string &path);

} // namespace odr::internal::util::file
