#ifndef ODR_ACCESS_FILE_UTIL_H
#define ODR_ACCESS_FILE_UTIL_H

#include <string>

namespace odr {
namespace access {

namespace FileUtil {
std::string read(const std::string &path);
void write(const std::string &content, const std::string &path);
} // namespace FileUtil

} // namespace access
} // namespace odr

#endif // ODR_ACCESS_FILE_UTIL_H
