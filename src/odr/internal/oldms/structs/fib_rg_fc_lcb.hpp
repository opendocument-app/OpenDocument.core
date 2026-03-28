#pragma once

#include <odr/internal/oldms/structs/fib_rg_fc_lcb_97.hpp>

#include <iosfwd>
#include <memory>

namespace odr::internal::oldms {

std::unique_ptr<FibRgFcLcb97> read_fib_rg_fc_lcb(std::istream &in,
                                                 std::uint16_t nFib);

} // namespace odr::internal::oldms
