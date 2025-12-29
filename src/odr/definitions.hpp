#pragma once

#include <cstdint>

namespace odr::internal::abstract {
class ElementAdapter;
} // namespace odr::internal::abstract

namespace odr {

using ElementIdentifier = std::uint64_t;

static constexpr ElementIdentifier null_element_id{0};

struct ElementHandle final {
  const internal::abstract::ElementAdapter *adapter_ptr{nullptr};
  ElementIdentifier identifier{null_element_id};

  ElementHandle() = default;
  ElementHandle(const internal::abstract::ElementAdapter *adapter_ptr_,
                const ElementIdentifier identifier_)
      : adapter_ptr(adapter_ptr_), identifier(identifier_) {}
  ElementHandle(const internal::abstract::ElementAdapter &adapter_,
                const ElementIdentifier identifier_)
      : adapter_ptr(&adapter_), identifier(identifier_) {}

  [[nodiscard]] const internal::abstract::ElementAdapter &adapter() const {
    return *adapter_ptr;
  }

  [[nodiscard]] bool is_null() const { return identifier == null_element_id; }

  bool operator==(const ElementHandle &other) const {
    return adapter_ptr == other.adapter_ptr && identifier == other.identifier;
  }
};

} // namespace odr
