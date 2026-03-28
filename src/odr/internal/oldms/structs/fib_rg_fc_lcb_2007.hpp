#pragma once

#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2003.hpp>

namespace odr::internal::oldms {

#pragma pack(push, 1)

struct FibRgFcLcb2007 : FibRgFcLcb2003 {
  FcLcb plcfmthd;
  FcLcb sttbfBkmkMoveFrom;
  FcLcb plcfBkfMoveFrom;
  FcLcb plcfBklMoveFrom;
  FcLcb sttbfBkmkMoveTo;
  FcLcb plcfBkfMoveTo;
  FcLcb plcfBklMoveTo;
  FcLcb unused1;
  FcLcb unused2;
  FcLcb unused3;
  FcLcb sttbfBkmkArto;
  FcLcb plcfBkfArto;
  FcLcb plcfBklArto;
  FcLcb artoData;
  FcLcb unused4;
  FcLcb unused5;
  FcLcb unused6;
  FcLcb ossTheme;
  FcLcb colorSchemeMapping;
};

#pragma pack(pop)

void read(std::istream &in, FibRgFcLcb2007 &fib_base);

} // namespace odr::internal::oldms
