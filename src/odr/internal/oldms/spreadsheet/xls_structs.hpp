#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

namespace odr::internal::oldms::spreadsheet {

// Filled by copying file bytes straight in (see xls_io), so multi-byte fields
// use host byte order — correct only on little-endian hosts (see xls_io.hpp).
// Bit-fields additionally assume LSB-first allocation, which all supported
// compilers use on little-endian targets (shared oldms/ assumption).

/// BIFF8 record type values handled here ([MS-XLS] 2.3 Record Enumeration).
enum BiffRecordType : std::uint16_t {
  biff_formula = 0x0006,    //< [MS-XLS] 2.4.127
  biff_eof = 0x000A,        //< [MS-XLS] 2.4.103
  biff_continue = 0x003C,   //< [MS-XLS] 2.4.58
  biff_boundsheet = 0x0085, //< [MS-XLS] 2.4.28 BoundSheet8
  biff_mulrk = 0x00BD,      //< [MS-XLS] 2.4.175
  biff_mulblank = 0x00BE,   //< [MS-XLS] 2.4.174
  biff_sst = 0x00FC,        //< [MS-XLS] 2.4.265
  biff_labelsst = 0x00FD,   //< [MS-XLS] 2.4.149
  biff_dimensions = 0x0200, //< [MS-XLS] 2.4.90
  biff_blank = 0x0201,      //< [MS-XLS] 2.4.20
  biff_number = 0x0203,     //< [MS-XLS] 2.4.180
  biff_label = 0x0204,      //< [MS-XLS] 2.4.148
  biff_boolerr = 0x0205,    //< [MS-XLS] 2.4.24
  biff_string = 0x0207,     //< [MS-XLS] 2.4.268
  biff_rk = 0x027E,         //< [MS-XLS] 2.4.220
  biff_bof = 0x0809,        //< [MS-XLS] 2.4.21
};

/// FormulaValue type discriminator when fExprO == 0xFFFF ([MS-XLS] 2.5.133).
enum FormulaValueType : std::uint8_t {
  formula_value_string = 0x00, //< value follows in a String record
  formula_value_boolean = 0x01,
  formula_value_error = 0x02,
  formula_value_blank = 0x03,
};

#pragma pack(push, 1)

/// Every record is `type, size, byte data[size]` ([MS-XLS] 2.1.4).
struct RecordHeader {
  std::uint16_t type;
  std::uint16_t size;
};
static_assert(sizeof(RecordHeader) == 4);

/// Head of the BOF record body ([MS-XLS] 2.4.21); the remaining build/version
/// flag fields are skipped.
struct BofFixed {
  std::uint16_t vers; //< 0x0600 for BIFF8
  std::uint16_t dt;   //< substream type: 0x0005 globals, 0x0010 sheet
};
static_assert(sizeof(BofFixed) == 4);

constexpr std::uint16_t bof_vers_biff8 = 0x0600;

/// Fixed head of the BoundSheet8 record body ([MS-XLS] 2.4.28); the sheet name
/// (a ShortXLUnicodeString) follows.
struct BoundSheet8Fixed {
  std::uint32_t lbPlyPos; //< stream offset of the sheet substream's BOF record
  std::uint8_t hsState : 2; //< hidden state
  std::uint8_t unused : 6;
  std::uint8_t dt; //< sheet type
};
static_assert(sizeof(BoundSheet8Fixed) == 6);

/// BoundSheet8.dt: only worksheets are parsed (0x01 macro, 0x02 chart,
/// 0x06 VBA module are skipped).
constexpr std::uint8_t boundsheet_dt_worksheet = 0x00;

/// Cell structure: the head of every cell record body ([MS-XLS] 2.5.13).
struct CellRef {
  std::uint16_t rw;
  std::uint16_t col;
  std::uint16_t ixfe;
};
static_assert(sizeof(CellRef) == 6);

/// Dimensions record body ([MS-XLS] 2.4.90).
struct DimensionsBody {
  std::uint32_t rwMic;  //< first used row
  std::uint32_t rwMac;  //< one past the last used row; 0 = no used cells
  std::uint16_t colMic; //< first used column
  std::uint16_t colMac; //< one past the last used column; 0 = no used cells
  std::uint16_t reserved;
};
static_assert(sizeof(DimensionsBody) == 14);

/// LabelSst record body ([MS-XLS] 2.4.149).
struct LabelSstBody {
  CellRef cell;
  std::uint32_t isst; //< index into the SST string array
};
static_assert(sizeof(LabelSstBody) == 10);

/// An RK-encoded number ([MS-XLS] 2.5.217).
struct RkNumber {
  std::uint32_t fX100 : 1; //< value is divided by 100
  std::uint32_t fInt : 1;  //< num is a signed integer, else Xnum high bits
  std::int32_t num : 30;   //< the integer, or the high 30 bits of a double

  [[nodiscard]] double decode() const {
    double value;
    if (fInt != 0) {
      value = num;
    } else {
      // num is the high 30 bits of an IEEE double, the rest is zero. The cast
      // chain keeps num's 30 bits and shifts them to the top; the two
      // sign-extension bits fall off the end.
      const std::uint64_t bits = static_cast<std::uint64_t>(num) << 34;
      value = std::bit_cast<double>(bits);
    }
    return fX100 != 0 ? value / 100.0 : value;
  }
};
static_assert(sizeof(RkNumber) == 4);

/// RK record body ([MS-XLS] 2.4.220): rw, col, then an RkRec ([MS-XLS]
/// 2.5.218) = ixfe + RK-encoded number.
struct RkBody {
  std::uint16_t rw;
  std::uint16_t col;
  std::uint16_t ixfe;
  RkNumber rk;
};
static_assert(sizeof(RkBody) == 10);

/// Number record body ([MS-XLS] 2.4.180): an IEEE-754 double (Xnum).
struct NumberBody {
  CellRef cell;
  double num;
};
static_assert(sizeof(NumberBody) == 14);

/// BoolErr record body ([MS-XLS] 2.4.24).
struct BoolErrBody {
  CellRef cell;
  std::uint8_t bBoolErr; //< boolean value or BErr error code, per fError
  std::uint8_t fError;   //< 0 = boolean, 1 = error
};
static_assert(sizeof(BoolErrBody) == 8);

/// FormulaValue ([MS-XLS] 2.5.133): the 8 bytes are an Xnum double unless
/// fExprO == 0xFFFF, then bytes[0] is a FormulaValueType and bytes[2] the
/// bool/error value.
struct FormulaValue {
  std::array<std::uint8_t, 6> bytes;
  std::uint16_t fExprO;

  [[nodiscard]] bool is_xnum() const { return fExprO != 0xFFFF; }
  [[nodiscard]] double as_xnum() const {
    if (!is_xnum()) {
      throw std::runtime_error("xls: formula value is not an Xnum");
    }
    return std::bit_cast<double>(*this);
  }
  [[nodiscard]] FormulaValueType type() const {
    if (is_xnum()) {
      throw std::runtime_error("xls: formula value is an Xnum, not a type");
    }
    return static_cast<FormulaValueType>(bytes[0]);
  }
  [[nodiscard]] std::uint8_t bool_err_value() const {
    if (is_xnum()) {
      throw std::runtime_error(
          "xls: formula value is an Xnum, not a bool/error");
    }
    return bytes[2];
  }
};
static_assert(sizeof(FormulaValue) == 8);

/// Fixed part of the Formula record body ([MS-XLS] 2.4.127); the parsed
/// formula expression follows and is skipped. The flag bits are not consumed.
struct FormulaFixed {
  CellRef cell;
  FormulaValue val;
  std::uint16_t fAlwaysCalc : 1;
  std::uint16_t reserved1 : 1;
  std::uint16_t fFill : 1;
  std::uint16_t fShrFmla : 1;
  std::uint16_t reserved2 : 1;
  std::uint16_t fClearErrors : 1;
  std::uint16_t reserved3 : 10;
  std::uint32_t chn;
};
static_assert(sizeof(FormulaFixed) == 20);

/// Head of the SST record body ([MS-XLS] 2.4.265); cstUnique
/// XLUnicodeRichExtendedString structures follow, potentially spilling into
/// CONTINUE records.
struct SstHead {
  std::int32_t cstTotal;
  std::int32_t cstUnique;
};
static_assert(sizeof(SstHead) == 8);

/// Option flags byte of the XLUnicodeString family ([MS-XLS] 2.5.240/293/294).
/// fExtSt/fRichSt exist only in the rich extended variant; plain strings have
/// those bits reserved as zero.
struct UnicodeStringFlags {
  std::uint8_t fHighByte : 1; //< 2 bytes per character (UTF-16LE), else 1
  std::uint8_t reserved : 1;
  std::uint8_t fExtSt : 1;  //< phonetic data follows the characters
  std::uint8_t fRichSt : 1; //< formatting runs follow the characters
  std::uint8_t unused : 4;
};
static_assert(sizeof(UnicodeStringFlags) == 1);

#pragma pack(pop)

} // namespace odr::internal::oldms::spreadsheet
