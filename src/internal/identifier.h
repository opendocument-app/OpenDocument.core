#ifndef ODR_INTERNAL_IDENTIFIER_H
#define ODR_INTERNAL_IDENTIFIER_H

#include <cstdint>

namespace odr::internal {

template <typename Number, typename Tag> struct Identifier {
  Identifier() = default;
  Identifier(const Number id) : id{id} {}

  operator bool() const { return id != 0; }
  operator Number() const { return id; }

  Number id{0};
};

struct ElementIdentifierTag {};

using ElementIdentifier = Identifier<std::uint64_t, ElementIdentifierTag>;

} // namespace odr::internal

#endif // ODR_INTERNAL_IDENTIFIER_H
