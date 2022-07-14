#ifndef ODR_INTERNAL_CSV_UTIL_H
#define ODR_INTERNAL_CSV_UTIL_H

#include <iosfwd>

namespace odr::internal::csv {

void check_csv_file(std::istream &in);

}

#endif // ODR_INTERNAL_CSV_UTIL_H
