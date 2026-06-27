#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace odr::internal::pdf {

class Object;

/// A PDF function object (ISO 32000-1 7.10): a black-box mapping from `m` input
/// values to `n` output values. The four sampled/analytic flavours
/// (type 0 sampled, type 2 exponential, type 3 stitching, type 4 PostScript
/// calculator) share one interface. Inputs are clipped to `/Domain` and outputs
/// to `/Range` (when declared) by `eval`; the type-specific math lives in the
/// concrete subclasses.
class Function {
public:
  virtual ~Function() = default;

  /// Evaluate the function: clip `in` to the domain, compute, clip the result
  /// to the range (when one is declared). `in` should carry `input_arity`
  /// values; a short input is zero-padded, a long one truncated.
  [[nodiscard]] std::vector<double> eval(std::vector<double> in) const;

  [[nodiscard]] std::size_t input_arity() const { return m_domain.size() / 2; }
  /// Declared output arity, or 0 when the function carries no `/Range` (only
  /// type 0 and 4 must; type 2/3 may omit it).
  [[nodiscard]] std::size_t output_arity() const { return m_range.size() / 2; }

protected:
  Function(std::vector<double> domain, std::vector<double> range)
      : m_domain{std::move(domain)}, m_range{std::move(range)} {}

  [[nodiscard]] virtual std::vector<double>
  compute(const std::vector<double> &in) const = 0;

  std::vector<double> m_domain; // [min0 max0 min1 max1 ...]
  std::vector<double> m_range;  // [min0 max0 ...], possibly empty
};

/// How `parse_function` reaches data behind indirect references: `resolve`
/// dereferences an object to a direct value (identity for a direct one);
/// `load_stream` returns the filter-decoded bytes of a stream object (used by
/// the sampled type 0 and the PostScript type 4). Decoupling these from the
/// document parser keeps `Function` pure and unit-testable.
struct FunctionContext {
  std::function<Object(const Object &)> resolve;
  std::function<std::string(const Object &)> load_stream;
};

/// Build a function from its PDF object (a dictionary, or a stream for type
/// 0/4; may be an indirect reference). Returns `nullptr` for an unsupported or
/// malformed function type.
std::shared_ptr<Function> parse_function(const Object &object,
                                         const FunctionContext &context);

} // namespace odr::internal::pdf
