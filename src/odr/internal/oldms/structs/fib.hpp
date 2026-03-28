#pragma once

#include <array>
#include <iosfwd>
#include <string>

#include <odr/internal/oldms/structs/fib_base.hpp>
#include <odr/internal/oldms/structs/fib_rg_csw_new.hpp>
#include <odr/internal/oldms/structs/fib_rg_fc_lcb_97.hpp>

namespace odr::internal::oldms {

struct Fib {
  FibBase base;
  std::uint16_t csw;
  std::array<std::uint16_t, 14> fibRgW;
  std::uint16_t cslw;
  std::array<std::uint16_t, 44> fibRgLw;
  std::uint16_t cbRgFcLcb;
  std::unique_ptr<FibRgFcLcb97> fibRgFcLcb;
  std::uint16_t cswNew;
  FibRgCswNew fibRgCswNew;
};

void read(std::istream &in, Fib &fib);

} // namespace odr::internal::oldms
