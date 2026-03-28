#include <odr/internal/oldms/structs/fib_rg_fc_lcb_97.hpp>

#include <odr/internal/util/byte_stream_util.hpp>

namespace odr::internal {

static_assert(sizeof(oldms::FibRgFcLcb97) == 744,
              "FibRgFcLcb97 should be 744 bytes in size");

void oldms::read(std::istream &in, FibRgFcLcb97 &fib_base) {
  util::byte_stream::read(in, fib_base);
}

} // namespace odr::internal
