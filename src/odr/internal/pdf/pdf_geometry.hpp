#pragma once

#include <array>

namespace odr::internal::pdf {

/// 2-D affine transform in PDF's convention: the matrix `[a b c d e f]` denotes
///
///     | a b 0 |
///     | c d 0 |
///     | e f 1 |
///
/// and points are row vectors multiplied on the left: `[x y 1] * M`.
/// Composition follows the same order, so `a * b` is "apply `a`, then `b`" (ISO
/// 32000-1 8.3.4).
struct Matrix {
  double a{1}, b{0}, c{0}, d{1}, e{0}, f{0};

  Matrix() = default;
  Matrix(double a, double b, double c, double d, double e, double f)
      : a{a}, b{b}, c{c}, d{d}, e{e}, f{f} {}

  static Matrix translation(double x, double y) { return {1, 0, 0, 1, x, y}; }
  static Matrix scaling(double x, double y) { return {x, 0, 0, y, 0, 0}; }

  /// `*this` applied first, then `rhs`.
  [[nodiscard]] Matrix operator*(const Matrix &rhs) const {
    return {
        a * rhs.a + b * rhs.c,         a * rhs.b + b * rhs.d,
        c * rhs.a + d * rhs.c,         c * rhs.b + d * rhs.d,
        e * rhs.a + f * rhs.c + rhs.e, e * rhs.b + f * rhs.d + rhs.f,
    };
  }

  /// Map the point `(x, y)` through the transform.
  [[nodiscard]] std::array<double, 2> apply(double x, double y) const {
    return {a * x + c * y + e, b * x + d * y + f};
  }
};

} // namespace odr::internal::pdf
