#pragma once

#include <odr/internal/oldms/structs/fib_rg_fc_lcb_97.hpp>

namespace odr::internal::oldms {

#pragma pack(push, 1)

struct FibRgFcLcb2000 : FibRgFcLcb97 {
  FcLcb plcfTch;
  FcLcb rmdThreading;
  FcLcb mid;
  FcLcb sttbRgtplc;
  FcLcb msoEnvelope;
  FcLcb plcfLad;
  FcLcb rgDofr;
  FcLcb plcosl;
  FcLcb plcfCookieOld;
  FcLcb pgdMotherOld;
  FcLcb bkdMotherOld;
  FcLcb pgdFtnOld;
  FcLcb bkdFtnOld;
  FcLcb pgdEdnOld;
  FcLcb bkdEdnOld;
};

#pragma pack(pop)

void read(std::istream &in, FibRgFcLcb2000 &fib_base);

} // namespace odr::internal::oldms
