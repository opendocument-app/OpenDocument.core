#include <odr/internal/oldms/structs/fib_rg_fc_lcb.hpp>

#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2000.hpp>
#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2002.hpp>
#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2003.hpp>
#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2007.hpp>
#include <odr/internal/oldms/structs/fib_rg_fc_lcb_97.hpp>
#include <odr/internal/oldms/structs/n_fib_values.hpp>
#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

std::unique_ptr<oldms::FibRgFcLcb97>
oldms::read_fib_rg_fc_lcb(std::istream &in, const std::uint16_t nFib) {
  switch (nFib) {
  case nFib97: {
    auto result = std::make_unique<FibRgFcLcb97>();
    read(in, *result);
    return result;
  }
  case nFib2000: {
    auto result = std::make_unique<FibRgFcLcb2000>();
    read(in, *result);
    return result;
  }
  case nFib2002: {
    auto result = std::make_unique<FibRgFcLcb2002>();
    read(in, *result);
    return result;
  }
  case nFib2003: {
    auto result = std::make_unique<FibRgFcLcb2003>();
    read(in, *result);
    return result;
  }
  case nFib2007: {
    auto result = std::make_unique<FibRgFcLcb2007>();
    read(in, *result);
    return result;
  }
  default:
    throw std::runtime_error("Unknown nFib value: " + std::to_string(nFib));
  }
}

} // namespace odr::internal
