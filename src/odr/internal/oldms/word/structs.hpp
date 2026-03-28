#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace odr::internal::oldms {

#pragma pack(push, 1)

enum NFibValues : std::uint16_t {
  nFib97 = 0x00C1,
  nFib2000 = 0x00D9,
  nFib2002 = 0x0101,
  nFib2003 = 0x010C,
  nFib2007 = 0x0112,
};

struct FcLcb {
  std::uint32_t fc;
  std::uint32_t lcb;
};

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

struct FibRgCswNewData2000 {
  std::uint16_t cQuickSavesNew;
};

struct FibRgCswNewData2007 : FibRgCswNewData2000 {
  std::uint16_t lidThemeOther;
  std::uint16_t lidThemeFE;
  std::uint16_t lidThemeCS;
};

struct FibRgCswNew {
  std::uint16_t nFibNew;
  std::unique_ptr<FibRgCswNewData2000> rgCswNewData;
};

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

#pragma pack(pop)

static_assert(sizeof(FcLcb) == 8, "FcLcb should be 8 bytes");
static_assert(sizeof(FibBase) == 32, "FibBase should be 8 bytes");
static_assert(sizeof(FibRgFcLcb97) == 744,
              "FibRgFcLcb97 should be 744 bytes in size");
static_assert(sizeof(FibRgFcLcb2000) == 864,
              "FibRgFcLcb2003 should be 864 bytes in size");
static_assert(sizeof(FibRgFcLcb2002) == 1088,
              "FibRgFcLcb2003 should be 1088 bytes in size");
static_assert(sizeof(FibRgFcLcb2003) == 1312,
              "FibRgFcLcb2003 should be 1312 bytes in size");
static_assert(sizeof(FibRgFcLcb2007) == 1464,
              "FibRgFcLcb2007 should be 1464 bytes in size");
static_assert(sizeof(FibRgCswNewData2000) == 2,
              "FibRgCswNewData2000 should be 2 bytes");
static_assert(sizeof(FibRgCswNewData2007) == 8,
              "FibRgCswNewData2007 should be 8 bytes");

} // namespace odr::internal::oldms
