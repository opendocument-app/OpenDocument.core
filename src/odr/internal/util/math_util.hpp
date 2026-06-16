#pragma once

#include <array>

namespace odr::internal::util::math {

/// 2-D affine transform using PDF's row-vector convention: `[a b c d e f]`
/// denotes
///
///     | a b 0 |
///     | c d 0 |
///     | e f 1 |
///
/// Points are row vectors multiplied on the left: `[x y 1] * M`. Composition
/// follows the same order: `a * b` means "apply `a`, then `b`" (ISO 32000-1
/// 8.3.4).
struct Transform2D {
  double a{1};
  double b{0};
  double c{0};
  double d{1};
  double e{0};
  double f{0};

  constexpr Transform2D() noexcept = default;
  constexpr Transform2D(const double a_, const double b_, const double c_,
                        const double d_, const double e_,
                        const double f_) noexcept
      : a{a_}, b{b_}, c{c_}, d{d_}, e{e_}, f{f_} {}

  constexpr static Transform2D translation(const double x,
                                           const double y) noexcept {
    return {1, 0, 0, 1, x, y};
  }
  constexpr static Transform2D scaling(const double x,
                                       const double y) noexcept {
    return {x, 0, 0, y, 0, 0};
  }
  /// `scaling(sx, sy)` then `translation(x, y)`: the point is scaled first and
  /// the translation `(x, y)` is applied in the output space (it is not
  /// scaled).
  constexpr static Transform2D scaling_translation(const double sx,
                                                   const double sy,
                                                   const double x,
                                                   const double y) noexcept {
    return {sx, 0, 0, sy, x, y};
  }

  /// `*this` applied first, then `rhs`.
  [[nodiscard]] constexpr Transform2D
  operator*(const Transform2D &rhs) const noexcept {
    return {
        a * rhs.a + b * rhs.c,         a * rhs.b + b * rhs.d,
        c * rhs.a + d * rhs.c,         c * rhs.b + d * rhs.d,
        e * rhs.a + f * rhs.c + rhs.e, e * rhs.b + f * rhs.d + rhs.f,
    };
  }

  /// Map the point `(x, y)` through the transform.
  [[nodiscard]] constexpr std::array<double, 2>
  apply(const double x, const double y) const noexcept {
    return {a * x + c * y + e, b * x + d * y + f};
  }
};

} // namespace odr::internal::util::math
