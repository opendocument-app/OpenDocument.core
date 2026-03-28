#pragma once

#include <cstdint>

namespace odr::internal::oldms {

#pragma pack(push, 1)

struct FcLcb {
  std::uint32_t fc;
  std::uint32_t lcb;
};

#pragma pack(pop)

} // namespace odr::internal::oldms
