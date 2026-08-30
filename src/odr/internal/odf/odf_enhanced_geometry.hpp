#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::odf {

/// What a `draw:enhanced-geometry` formula can name (20.36) besides its own
/// equations. The defaults are ODF's own 21600 square.
struct EnhancedGeometryContext final {
  double left{0};
  double top{0};
  double right{21600};
  double bottom{21600};
  /// What `logwidth` and `logheight` name, in the view box's own units.
  double logical_width{21600};
  double logical_height{21600};
  double x_stretch{0};
  double y_stretch{0};
  bool has_stroke{true};
  bool has_fill{true};
  /// `draw:modifiers`, which `$0`, `$1`, … index.
  std::vector<double> modifiers;
};

/// Resolves a `?name` reference to the `draw:equation` it names.
using EquationResolver =
    std::function<std::optional<double>(std::string_view name)>;

/// `draw:formula` (20.36). Nothing where it does not parse, or names something
/// that does not resolve.
[[nodiscard]] std::optional<double>
evaluate_formula(std::string_view formula,
                 const EnhancedGeometryContext &context,
                 const EquationResolver &equations);

/// `draw:enhanced-path` (19.145) as an svg `d`. Nothing where it does not
/// parse, or names a value that does not resolve; `F` and `S` are dropped.
[[nodiscard]] std::optional<std::string>
convert_enhanced_path(std::string_view path,
                      const EnhancedGeometryContext &context,
                      const EquationResolver &equations);

} // namespace odr::internal::odf
