#include <odr/internal/util/math_util.hpp>

#include <gtest/gtest.h>

using namespace odr::internal::util::math;

TEST(Transform2D, identity_apply) {
  const auto p = Transform2D().apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 3);
  EXPECT_DOUBLE_EQ(p[1], 4);
}

TEST(Transform2D, translation_apply) {
  const auto p = Transform2D::translation(10, -5).apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 13);
  EXPECT_DOUBLE_EQ(p[1], -1);
}

TEST(Transform2D, scaling_apply) {
  const auto p = Transform2D::scaling(2, 3).apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 6);
  EXPECT_DOUBLE_EQ(p[1], 12);
}

// `a * b` applies `a` first, then `b` (row-vector convention), so order
// matters.
TEST(Transform2D, compose_is_ordered) {
  // scale, then translate
  const auto p =
      (Transform2D::scaling(2, 2) * Transform2D::translation(1, 1)).apply(3, 4);
  EXPECT_DOUBLE_EQ(p[0], 7); // (3,4) -> (6,8) -> (7,9)
  EXPECT_DOUBLE_EQ(p[1], 9);

  // translate, then scale -> different result
  const auto q =
      (Transform2D::translation(1, 1) * Transform2D::scaling(2, 2)).apply(3, 4);
  EXPECT_DOUBLE_EQ(q[0], 8); // (3,4) -> (4,5) -> (8,10)
  EXPECT_DOUBLE_EQ(q[1], 10);
}

// Composing then applying equals applying each factor in sequence.
TEST(Transform2D, compose_matches_sequential_apply) {
  const Transform2D a{1, 2, 3, 4, 5, 6};
  const Transform2D b{2, 0, 1, 3, -1, 4};

  const auto direct = (a * b).apply(7, 8);
  const auto step = a.apply(7, 8);
  const auto seq = b.apply(step[0], step[1]);

  EXPECT_DOUBLE_EQ(direct[0], seq[0]);
  EXPECT_DOUBLE_EQ(direct[1], seq[1]);
}
