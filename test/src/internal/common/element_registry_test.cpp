#include <odr/internal/common/element_registry.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace odr::internal;

TEST(SortedSideTable, rejects_wide_ids_without_corrupting_order) {
  SortedSideTable<std::uint32_t, std::uint8_t> table;
  table.emplace(1, 10);
  table.emplace(255, 20);
  EXPECT_THROW(table.emplace(256, 30), std::out_of_range);
  EXPECT_THROW(
      table.emplace(std::numeric_limits<odr::ElementIdentifier>::max(), 40),
      std::out_of_range);
  EXPECT_EQ(table.at(1), 10);
  EXPECT_EQ(table.at(255), 20);
  EXPECT_EQ(table.find(256), nullptr);
  EXPECT_EQ(table.find(0), nullptr);
}
