#pragma once

#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2000.hpp>

namespace odr::internal::oldms {

#pragma pack(push, 1)

struct FibRgFcLcb2002 : FibRgFcLcb2000 {
  FcLcb unused1;
  FcLcb plcfPgp;
  FcLcb plcfuim;
  FcLcb plfguidUim;
  FcLcb atrdExtra;
  FcLcb plrsid;
  FcLcb sttbfBkmkFactoid;
  FcLcb plcfBkfFactoid;
  FcLcb plcfcookie;
  FcLcb plcfBklFactoid;
  FcLcb factoidData;
  FcLcb docUndo;
  FcLcb sttbfBkmkFcc;
  FcLcb plcfBkfFcc;
  FcLcb plcfBklFcc;
  FcLcb sttbfbkmkBPRepairs;
  FcLcb plcfbkfBPRepairs;
  FcLcb plcfbklBPRepairs;
  FcLcb pmsNew;
  FcLcb odso;
  FcLcb plcfpmiOldXP;
  FcLcb plcfpmiNewXP;
  FcLcb plcfpmiMixedXP;
  FcLcb unused2;
  FcLcb plcffactoid;
  FcLcb plcflvcOldXP;
  FcLcb plcflvcNewXP;
  FcLcb plcflvcMixedXP;
};

#pragma pack(pop)

void read(std::istream &in, FibRgFcLcb2002 &fib_base);

} // namespace odr::internal::oldms
