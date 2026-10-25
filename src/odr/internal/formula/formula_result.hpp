#pragma once

#include <odr/internal/formula/formula_value.hpp>

#include <expected>
#include <optional>

namespace odr::internal::formula {

/// What ends an evaluation early: no answer, or an argument error that
/// becomes the result of its function.
struct Stop final {
  /// The error the function gives, nothing where there is no answer.
  std::optional<ErrorType> error{};
};

template <typename T> using Result = std::expected<T, Stop>;

/// The stop of an indeterminate result: `evaluate` gives nothing.
[[nodiscard]] inline std::unexpected<Stop> no_answer() noexcept {
  return std::unexpected(Stop{});
}

/// The stop of an argument error: @p error becomes the result of its
/// function.
[[nodiscard]] inline std::unexpected<Stop>
error_result(const ErrorType error) noexcept {
  return std::unexpected(Stop{error});
}

} // namespace odr::internal::formula
