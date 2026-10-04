#pragma once

#include <odr/internal/util/byte_string.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace odr::internal::oldms::text {

// Filled by copying file bytes straight in (see doc_io): little-endian,
// LSB-first hosts only — see oldms/AGENTS.md.

/// FibBase.wIdent of every word binary document ([MS-DOC] 2.5.2).
constexpr std::uint16_t fib_wIdent = 0xA5EC;

enum NFibValues : std::uint16_t {
  nFib97 = 0x00C1,
  nFib2000 = 0x00D9,
  nFib2002 = 0x0101,
  nFib2003 = 0x010C,
  nFib2007 = 0x0112,
};

#pragma pack(push, 1)

struct FcLcb {
  std::uint32_t fc;
  std::uint32_t lcb;
};
static_assert(sizeof(FcLcb) == 8, "FcLcb should be 8 bytes");

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
static_assert(sizeof(FibBase) == 32, "FibBase should be 32 bytes");

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
static_assert(sizeof(FibRgFcLcb97) == 744,
              "FibRgFcLcb97 should be 744 bytes in size");

/// The character SPRMs mapped to TextStyle ([MS-DOC] 2.6.1); everything else
/// in a Chpx is skipped via Sprm::operand_size.
enum CharacterSprms : std::uint16_t {
  sprmCFBold = 0x0835,     //< ToggleOperand
  sprmCFItalic = 0x0836,   //< ToggleOperand
  sprmCFStrike = 0x0837,   //< ToggleOperand
  sprmCHighlight = 0x2A0C, //< Ico
  sprmCKul = 0x2A3E,       //< Kul; 0x00 = none
  sprmCIco = 0x2A42,       //< Ico (legacy palette color)
  sprmCHps = 0x4A43,       //< u16 half-points (default 20)
  sprmCRgFtc0 = 0x4A4F,    //< s16 index into SttbfFfn
  sprmCCv = 0x6870,        //< COLORREF
};

struct Sprm {
  std::uint16_t ispmd : 9;
  std::uint16_t fSpec : 1;
  std::uint16_t sgc : 3;
  std::uint16_t spra : 3;

  [[nodiscard]] std::int32_t operand_size() const {
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
static_assert(sizeof(Sprm) == 2, "Sprm should be 2 bytes");

struct FcCompressed {
  std::uint32_t fc : 30;
  std::uint32_t fCompressed : 1;
  std::uint32_t r1 : 1;
};
static_assert(sizeof(FcCompressed) == 4, "FcCompressed should be 4 bytes");

struct Pcd {
  std::uint16_t fNoParaLast : 1;
  std::uint16_t fR1 : 1;
  std::uint16_t fDirty : 1;
  std::uint16_t fR : 13;
  FcCompressed fc;
  std::uint16_t prm;
};
static_assert(sizeof(Pcd) == 8, "Pcd should be 8 bytes");

/// Location of a ChpxFkp in the WordDocument stream, at pn * 512
/// ([MS-DOC] 2.9.206).
struct PnFkpChpx {
  std::uint32_t pn : 22;
  std::uint32_t unused : 10;
};
static_assert(sizeof(PnFkpChpx) == 4, "PnFkpChpx should be 4 bytes");

/// Fixed head of an FFN ([MS-DOC] 2.9.82); the null-terminated UTF-16 font
/// name (xszFfn) follows.
struct FfnFixed {
  std::uint8_t ffid;
  std::int16_t wWeight;
  std::uint8_t chs;
  std::uint8_t ixchSzAlt;
  std::array<std::uint8_t, 10> panose;
  std::array<std::uint8_t, 24> fs;
};
static_assert(sizeof(FfnFixed) == 39, "FfnFixed should be 39 bytes");

#pragma pack(pop)

struct ParsedFib {
  FibBase base;
  std::uint16_t csw;
  std::array<std::uint16_t, 14> fibRgW;
  std::uint16_t cslw;
  std::array<std::uint16_t, 44> fibRgLw;
  std::uint16_t cbRgFcLcb;
  FibRgFcLcb97 fibRgFcLcb{};
  std::uint16_t cswNew;
  std::optional<std::uint16_t> nFibNew;

  /// FibRgLw97.ccpText ([MS-DOC] 2.5.5), the 4th 32-bit field of fibRgLw.
  [[nodiscard]] std::int32_t ccpText() const {
    // Assembled unsigned: a signed shift would be UB with the sign bit set.
    const std::uint32_t value = static_cast<std::uint32_t>(fibRgLw[6]) |
                                (static_cast<std::uint32_t>(fibRgLw[7]) << 16);
    return static_cast<std::int32_t>(value);
  }
};

/// Zero-copy view over a PLC ([MS-DOC] 2.2.2): n+1 CPs followed by n data
/// elements, over a buffer that must outlive the view.
template <typename Data> class PlcMap {
public:
  PlcMap(const char *data, const std::size_t cbPlc)
      : m_data(data), m_cbPlc(cbPlc) {}

  /// Number of data elements; throws unless cbPlc yields a whole number.
  [[nodiscard]] std::size_t n() const {
    constexpr std::size_t stride = 4 + sizeof(Data);
    if (m_cbPlc < 4 || (m_cbPlc - 4) % stride != 0) {
      throw std::runtime_error("doc: malformed Plc size");
    }
    return (m_cbPlc - 4) / stride;
  }

  [[nodiscard]] std::uint32_t aCP(const std::size_t i) const {
    if (i > n()) {
      throw std::out_of_range("doc: PLC boundary index out of range");
    }
    util::byte_string::Reader cursor(std::string_view(m_data, m_cbPlc));
    cursor.skip(i * 4);
    return cursor.read<std::uint32_t>();
  }

  [[nodiscard]] Data aData(const std::size_t i) const {
    const std::size_t count = n();
    if (i >= count) {
      throw std::out_of_range("doc: PLC data index out of range");
    }
    util::byte_string::Reader cursor(std::string_view(m_data, m_cbPlc));
    cursor.skip((count + 1) * 4 + i * sizeof(Data));
    return cursor.read<Data>();
  }

private:
  const char *m_data{nullptr};
  std::size_t m_cbPlc{0};
};

/// PlcPcd ([MS-DOC] 2.8.35): the piece table, keyed by CP.
using PlcPcdMap = PlcMap<Pcd>;

/// PlcBteChpx ([MS-DOC] 2.8.5): keyed by WordDocument-stream offsets (fc, not
/// CP); its data elements locate the ChpxFkp pages.
using PlcBteChpxMap = PlcMap<PnFkpChpx>;

} // namespace odr::internal::oldms::text
