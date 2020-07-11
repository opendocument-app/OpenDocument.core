#ifndef ODR_ACCESS_STREAMUTIL_H
#define ODR_ACCESS_STREAMUTIL_H

#include <iostream>
#include <string>

namespace odr {
namespace access {

namespace StreamUtil {

extern std::string read(std::istream &);

extern void pipe(std::istream &, std::ostream &);

} // namespace StreamUtil

} // namespace access
} // namespace odr

#endif // ODR_ACCESS_STREAMUTIL_H
