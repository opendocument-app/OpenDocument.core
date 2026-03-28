#pragma once

#include <odr/internal/oldms/structs/fc_lcb.hpp>

#include <cstdint>
#include <iosfwd>

namespace odr::internal::oldms {

#pragma pack(push, 1)

struct FibRgFcLcb97 {
  FcLcb stshfOrig;
  FcLcb stshf;
  FcLcb plcffndRef;
  FcLcb plcffndTxt;
  FcLcb plcfandRef;
  FcLcb plcfandTxt;
  FcLcb plcfSed;
  FcLcb plcPad;
  FcLcb plcfPhe;
  FcLcb sttbfGlsy;
  FcLcb plcfGlsy;
  FcLcb plcfHdd;
  FcLcb plcfBteChpx;
  FcLcb plcfBtePapx;
  FcLcb plcfSea;
  FcLcb sttbfFfn;
  FcLcb plcfFldMom;
  FcLcb plcfFldHdr;
  FcLcb plcfFldFtn;
  FcLcb plcfFldAtn;
  FcLcb plcfFldMcr;
  FcLcb sttbfBkmk;
  FcLcb plcfBkf;
  FcLcb plcfBkl;
  FcLcb cmds;
  FcLcb unused1;
  FcLcb sttbfMcr;
  FcLcb prDrvr;
  FcLcb prEnvPort;
  FcLcb prEnvLand;
  FcLcb wss;
  FcLcb dop;
  FcLcb sttbfAssoc;
  FcLcb clx;
  FcLcb plcfPgdFtn;
  FcLcb autosaveSource;
  FcLcb grpXstAtnOwners;
  FcLcb sttbfAtnBkmk;
  FcLcb unused2;
  FcLcb unused3;
  FcLcb plcSpaMom;
  FcLcb plcSpaHdr;
  FcLcb plcfAtnBkf;
  FcLcb plcfAtnBkl;
  FcLcb pms;
  FcLcb formFldSttbs;
  FcLcb plcfendRef;
  FcLcb plcfendTxt;
  FcLcb plcfFldEdn;
  FcLcb unused4;
  FcLcb dggInfo;
  FcLcb sttbfRMark;
  FcLcb sttbfCaption;
  FcLcb sttbfAutoCaption;
  FcLcb plcfWkb;
  FcLcb plcfSpl;
  FcLcb plcftxbxTxt;
  FcLcb plcfFldTxbx;
  FcLcb plcfHdrtxbxTxt;
  FcLcb plcffldHdrTxbx;
  FcLcb stwUser;
  FcLcb sttbTtmbd;
  FcLcb cookieData;
  FcLcb pgdMotherOldOld;
  FcLcb bkdMotherOldOld;
  FcLcb pgdFtnOldOld;
  FcLcb bkdFtnOldOld;
  FcLcb pgdEdnOldOld;
  FcLcb bkdEdnOldOld;
  FcLcb sttbfIntlFld;
  FcLcb routeSlip;
  FcLcb sttbSavedBy;
  FcLcb sttbFnm;
  FcLcb plfLst;
  FcLcb plfLfo;
  FcLcb plcfTxbxBkd;
  FcLcb plcfTxbxHdrBkd;
  FcLcb docUndoWord9;
  FcLcb rgbUse;
  FcLcb usp;
  FcLcb uskf;
  FcLcb plcupcRgbUse;
  FcLcb plcupcUsp;
  FcLcb sttbGlsyStyle;
  FcLcb plgosl;
  FcLcb plcocx;
  FcLcb plcfBteLvc;
  std::uint32_t dwLowDateTime;
  std::uint32_t dwHighDateTime;
  FcLcb plcfLvcPre10;
  FcLcb plcfAsumy;
  FcLcb plcfGram;
  FcLcb sttbListNames;
  FcLcb sttbfUssr;
};

#pragma pack(pop)

void read(std::istream &in, FibRgFcLcb97 &fib_base);

} // namespace odr::internal::oldms
