#pragma once

#include <odr/internal/oldms/structs/fib_rg_fc_lcb_2002.hpp>

namespace odr::internal::oldms {

#pragma pack(push, 1)

struct FibRgFcLcb2003 : FibRgFcLcb2002 {
  FcLcb hplxsdr;
  FcLcb sttbfBkmkSdt;
  FcLcb plcfBkfSdt;
  FcLcb plcfBklSdt;
  FcLcb customXForm;
  FcLcb sttbfBkmkProt;
  FcLcb plcfBkfProt;
  FcLcb plcfBklProt;
  FcLcb sttbProtUser;
  FcLcb unused;
  FcLcb plcfpmiOld;
  FcLcb plcfpmiOldInline;
  FcLcb plcfpmiNew;
  FcLcb plcfpmiNewInline;
  FcLcb plcflvcOld;
  FcLcb plcflvcOldInline;
  FcLcb plcflvcNew;
  FcLcb plcflvcNewInline;
  FcLcb pgdMother;
  FcLcb bkdMother;
  FcLcb afdMother;
  FcLcb pgdFtn;
  FcLcb bkdFtn;
  FcLcb afdFtn;
  FcLcb pgdEdn;
  FcLcb bkdEdn;
  FcLcb afdEdn;
  FcLcb afd;
};

#pragma pack(pop)

void read(std::istream &in, FibRgFcLcb2003 &fib_base);

} // namespace odr::internal::oldms
