#ifndef ODR_INTERNAL_IDENTIFIER_H
#define ODR_INTERNAL_IDENTIFIER_H

#include <cstdint>
#include <functional>

namespace odr::internal {

template <typename Number, typename Tag> struct Identifier {
  Identifier() = default;
  Identifier(const Number id) : id{id} {}

  operator bool() const { return id != 0; }
  operator Number() const { return id; }

  bool operator==(const Identifier &rhs) const { return id == rhs.id; }
  bool operator!=(const Identifier &rhs) const { return id != rhs.id; }

  Number id{0};
};

struct ElementIdentifierTag {};

using ElementIdentifier = Identifier<std::uint64_t, ElementIdentifierTag>;

} // namespace odr::internal

template <typename Number, typename Tag>
struct std::hash<odr::internal::Identifier<Number, Tag>> {
  std::size_t
  operator()(const odr::internal::Identifier<Number, Tag> &k) const {
    static std::hash<Number> number_hasher;
    return number_hasher(k.id);
  }
};

#endif // ODR_INTERNAL_IDENTIFIER_H
