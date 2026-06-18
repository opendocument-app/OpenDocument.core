#include <odr/internal/pdf/pdf_file_object.hpp>

#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

TEST(Xref, append_keeps_newest_entry) {
  Xref newer;
  newer.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{100});

  Xref older;
  older.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{50});
  older.table.emplace(ObjectReference(2, 0), Xref::UsedEntry{60});

  newer.append(older);

  EXPECT_EQ(newer.table.at(ObjectReference(1, 0)).as_used().position, 100);
  EXPECT_EQ(newer.table.at(ObjectReference(2, 0)).as_used().position, 60);
}

TEST(Xref, merge_hybrid_fills_absent_and_free_only) {
  Xref classic;
  classic.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{100});
  classic.table.emplace(ObjectReference(2, 0), Xref::FreeEntry{0, 0});
  classic.table.emplace(ObjectReference(5, 65535), Xref::FreeEntry{0, 65535});

  Xref stream;
  stream.table.emplace(ObjectReference(1, 0), Xref::UsedEntry{999});
  stream.table.emplace(ObjectReference(2, 0), Xref::CompressedEntry{7, 0});
  stream.table.emplace(ObjectReference(3, 0), Xref::UsedEntry{300});
  stream.table.emplace(ObjectReference(5, 0), Xref::UsedEntry{500});

  classic.merge_hybrid(stream);

  // the classic in-use entry wins over the stream's
  EXPECT_EQ(classic.table.at(ObjectReference(1, 0)).as_used().position, 100);
  // hidden object: free in the classic table, live in the stream
  EXPECT_TRUE(classic.table.at(ObjectReference(2, 0)).is_compressed());
  // absent in the classic table
  EXPECT_EQ(classic.table.at(ObjectReference(3, 0)).as_used().position, 300);
  // free under a different generation key
  EXPECT_EQ(classic.table.at(ObjectReference(5, 0)).as_used().position, 500);
}
