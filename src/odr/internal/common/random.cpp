#include <odr/internal/common/random.hpp>

#include <algorithm>
#include <random>

namespace odr {

std::string internal::random_string(const std::size_t length) {
  static const std::string charset = "0123456789"
                                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                     "abcdefghijklmnopqrstuvwxyz";

  thread_local std::mt19937 generator(std::random_device{}());
  thread_local std::uniform_int_distribution<std::size_t> distribution(
      0, charset.size() - 1);

  std::string result;
  result.reserve(length);
  std::generate_n(std::back_inserter(result), length,
                  [] { return charset[distribution(generator)]; });

  return result;
}

} // namespace odr
