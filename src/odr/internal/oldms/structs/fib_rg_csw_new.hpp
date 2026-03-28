#pragma once

#include <cstdint>
#include <iosfwd>
#include <memory>

namespace odr::internal::oldms {

#pragma pack(push, 1)

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

#pragma pack(pop)

void read(std::istream &in, FibRgCswNew &out);

} // namespace odr::internal::oldms
