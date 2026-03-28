#pragma once

#include <cstdint>
#include <iosfwd>

namespace odr::internal::oldms {

#pragma pack(push, 1)

struct FibBase {
  std::uint16_t wIdent;
  std::uint16_t nFib;
  std::uint16_t unused;
  std::uint16_t lid;
  std::uint16_t pnNext;
  std::uint16_t flags1;
  std::uint16_t nFibBack;
  std::uint32_t lKey;
  std::uint8_t envr;
  std::uint8_t flags2;
  std::uint16_t reserved3;
  std::uint16_t reserved4;
  std::uint32_t reserved5;
  std::uint32_t reserved6;
};

#pragma pack(pop)

void read(std::istream &in, FibBase &fibBase);

} // namespace odr::internal::oldms
