#include <odr/internal/pdf/pdf_geometry.hpp>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

TEST(PdfGeometry, identity_apply) {
  const auto p = Matrix().apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 3);
  EXPECT_DOUBLE_EQ(p[1], 4);
}

TEST(PdfGeometry, translation_apply) {
  const auto p = Matrix::translation(10, -5).apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 13);
  EXPECT_DOUBLE_EQ(p[1], -1);
}

TEST(PdfGeometry, scaling_apply) {
  const auto p = Matrix::scaling(2, 3).apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 6);
  EXPECT_DOUBLE_EQ(p[1], 12);
}

// `a * b` applies `a` first, then `b` (row-vector convention), so order
// matters.
TEST(PdfGeometry, compose_is_ordered) {
  // scale, then translate
  const auto p =
      (Matrix::scaling(2, 2) * Matrix::translation(1, 1)).apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 7); // (3,4) -> (6,8) -> (7,9)
  EXPECT_DOUBLE_EQ(p[1], 9);

  // translate, then scale -> different result
  const auto q =
      (Matrix::translation(1, 1) * Matrix::scaling(2, 2)).apply(3, 4);
  EXPECT_DOUBLE_EQ(q[0], 8); // (3,4) -> (4,5) -> (8,10)
  EXPECT_DOUBLE_EQ(q[1], 10);
}

// Composing then applying equals applying each factor in sequence.
TEST(PdfGeometry, compose_matches_sequential_apply) {
  const Matrix a{1, 2, 3, 4, 5, 6};
  const Matrix b{2, 0, 1, 3, -1, 4};

  const auto direct = (a * b).apply(7, 8);
  const auto step = a.apply(7, 8);
  const auto seq = b.apply(step[0], step[1]);

  EXPECT_DOUBLE_EQ(direct[0], seq[0]);
  EXPECT_DOUBLE_EQ(direct[1], seq[1]);
}
