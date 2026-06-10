#pragma once

#include <array>
#include <cstdint>

namespace odr::internal::oldms::spreadsheet {

// Filled by copying file bytes straight in (see xls_io), so multi-byte fields
// use host byte order — correct only on little-endian hosts (see xls_io.hpp).

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
  std::uint8_t hsState;   //< hidden state in the low 2 bits
  std::uint8_t dt;        //< sheet type
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

/// RK record body ([MS-XLS] 2.4.220): rw, col, then an RkRec ([MS-XLS]
/// 2.5.218) = ixfe + RK-encoded number.
struct RkBody {
  std::uint16_t rw;
  std::uint16_t col;
  std::uint16_t ixfe;
  std::uint32_t rk;
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
/// fExprO == 0xFFFF, then bytes[0] is the value type (0 string in a following
/// String record, 1 boolean, 2 error, 3 blank) and bytes[2] the bool/error
/// value.
struct FormulaValue {
  std::array<std::uint8_t, 6> bytes;
  std::uint16_t fExprO;
};
static_assert(sizeof(FormulaValue) == 8);

/// Fixed part of the Formula record body ([MS-XLS] 2.4.127); the parsed
/// formula expression follows and is skipped.
struct FormulaFixed {
  CellRef cell;
  FormulaValue val;
  std::uint16_t flags;
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

#pragma pack(pop)

} // namespace odr::internal::oldms::spreadsheet
