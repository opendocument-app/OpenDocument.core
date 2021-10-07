#ifndef ODR_INTERNAL_HASH_UTIL_H
#define ODR_INTERNAL_HASH_UTIL_H

#include <cstdint>
#include <functional>

namespace odr::internal::util::hash {

void hash_combine(std::size_t &);

template <typename T, typename... Rest>
void hash_combine(std::size_t &seed, const T &v, Rest... rest) {
  static std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  hash_combine(seed, rest...);
}

} // namespace odr::internal::util::hash

#endif // ODR_INTERNAL_HASH_UTIL_H
