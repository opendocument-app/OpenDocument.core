#ifndef ODR_ACCESS_STREAMUTIL_H
#define ODR_ACCESS_STREAMUTIL_H

#include <iostream>
#include <string>

namespace odr::access::StreamUtil {

extern std::string read(std::istream &);

extern void pipe(std::istream &, std::ostream &);

} // namespace odr::access::StreamUtil

#endif // ODR_ACCESS_STREAMUTIL_H
