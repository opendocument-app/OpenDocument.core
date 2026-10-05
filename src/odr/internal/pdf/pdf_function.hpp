#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace odr::internal::pdf {

class Object;

/// PDF function types 0, 2, 3 and 4 (ISO 32000-1 7.10).
/// Evaluation clips inputs to `/Domain` and outputs to an optional `/Range`.
class Function {
public:
  virtual ~Function() = default;

  /// Evaluate after padding or truncating `in` to `input_arity` values.
  [[nodiscard]] std::vector<double> eval(std::vector<double> in) const;

  [[nodiscard]] std::size_t input_arity() const { return m_domain.size() / 2; }
  /// Declared output arity, or 0 when the function carries no `/Range` (only
  /// type 0 and 4 must; type 2/3 may omit it).
  [[nodiscard]] std::size_t output_arity() const { return m_range.size() / 2; }

protected:
  Function(std::vector<double> domain, std::vector<double> range)
      : m_domain{std::move(domain)}, m_range{std::move(range)} {}

  [[nodiscard]] virtual std::vector<double>
  compute(std::span<const double> in) const = 0;

  std::vector<double> m_domain; // [min0 max0 min1 max1 ...]
  std::vector<double> m_range;  // [min0 max0 ...], possibly empty
};

/// Resolves indirect objects and loads decoded stream bytes for types 0 and 4.
struct FunctionContext {
  std::function<Object(const Object &)> resolve;
  std::function<std::string(const Object &)> load_stream;
};

/// Parse a direct or indirect function object; return null for unsupported
/// types, invalid layouts or excessive nesting.
std::shared_ptr<Function> parse_function(const Object &object,
                                         const FunctionContext &context);

} // namespace odr::internal::pdf
