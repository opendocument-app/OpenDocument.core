#ifndef ODR_INTERNAL_PDF_CMAP_HPP
#define ODR_INTERNAL_PDF_CMAP_HPP

#include <unordered_map>

namespace odr::internal::pdf {

struct CMap {
  std::unordered_map<char, char16_t> bfchar;
};

} // namespace odr::internal::pdf

#endif // ODR_INTERNAL_PDF_CMAP_HPP
