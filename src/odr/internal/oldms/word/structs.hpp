#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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
  std::uint16_t fDot : 1;
  std::uint16_t fGlsy : 1;
  std::uint16_t fComplex : 1;
  std::uint16_t fHasPic : 1;
  std::uint16_t cQuickSaves : 4;
  std::uint16_t fEncrypted : 1;
  std::uint16_t fWhichTblStm : 1;
  std::uint16_t fReadOnlyRecommended : 1;
  std::uint16_t fWriteReservation : 1;
  std::uint16_t fExtChar : 1;
  std::uint16_t fLoadOverride : 1;
  std::uint16_t fFarEast : 1;
  std::uint16_t fObfuscated : 1;
  std::uint16_t nFibBack;
  std::uint32_t lKey;
  std::uint8_t envr;
  std::uint8_t fMac : 1;
  std::uint8_t fEmptySpecial : 1;
  std::uint8_t fLoadOverridePage : 1;
  std::uint8_t reserved1 : 1;
  std::uint8_t reserved2 : 1;
  std::uint8_t fSpare0 : 3;
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

struct Sprm {
  std::uint16_t ispmd : 9;
  std::uint16_t fSpec : 1;
  std::uint16_t sgc : 3;
  std::uint16_t spra : 3;

  int operand_size() const {
    switch (spra) {
    case 0:
    case 1:
      return 1;
    case 2:
    case 4:
    case 5:
      return 2;
    case 7:
      return 3;
    case 3:
      return 4;
    case 6:
      return -1;
    default:
      throw std::logic_error("Invalid spra value: " + std::to_string(spra));
    }
  }
};

struct FcCompressed {
  std::uint32_t fc : 30;
  std::uint32_t fCompressed : 1;
  std::uint32_t r1 : 1;
};

struct Pcd {
  std::uint16_t fNoParaLast : 1;
  std::uint16_t fR1 : 1;
  std::uint16_t fDirty : 1;
  std::uint16_t fR : 13;
  FcCompressed fc;
  std::uint16_t prm;
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
static_assert(sizeof(FcCompressed) == 4, "FcCompressed should be 4 bytes");
static_assert(sizeof(Pcd) == 8, "Pcd should be 8 bytes");

struct ParsedFibRgCswNew {
  std::uint16_t nFibNew;
  std::unique_ptr<FibRgCswNewData2000> rgCswNewData;
};

struct ParsedFib {
  FibBase base;
  std::uint16_t csw;
  std::array<std::uint16_t, 14> fibRgW;
  std::uint16_t cslw;
  std::array<std::uint16_t, 44> fibRgLw;
  std::uint16_t cbRgFcLcb;
  std::unique_ptr<FibRgFcLcb97> fibRgFcLcb;
  std::uint16_t cswNew;
  ParsedFibRgCswNew fibRgCswNew;
};

template <typename Derived, typename Data> class PlcBase {
public:
  static constexpr std::uint32_t cbData() { return sizeof(Data); }

  std::uint32_t n() const { return (self().cbPlc() - 4) / (4 + sizeof(Data)); }

  const std::uint32_t *aCP_ptr() const {
    return reinterpret_cast<const std::uint32_t *>(self().data());
  }

  const Data *aData_ptr() const {
    return reinterpret_cast<const Data *>(self().data() + (n() + 1) * 4);
  }

  std::uint32_t aCP(const std::uint32_t i) const { return aCP_ptr()[i]; }

  Data aData(const std::uint32_t i) const { return aData_ptr()[i]; }

private:
  Derived &self() { return *static_cast<Derived *>(this); }
  const Derived &self() const { return *static_cast<const Derived *>(this); }
};

template <typename Derived> class PlcPcdBase : public PlcBase<Derived, Pcd> {};

class PlcPcdMap : public PlcPcdBase<PlcPcdMap> {
public:
  PlcPcdMap(void *data, const std::size_t cbPlc)
      : m_data(data), m_cbPlc(cbPlc) {}

  void *data() const { return m_data; }
  std::size_t cbPlc() const { return m_cbPlc; }

private:
  void *m_data{nullptr};
  std::size_t m_cbPlc{0};
};

} // namespace odr::internal::oldms
