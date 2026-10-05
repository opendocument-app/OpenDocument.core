#include <odr/internal/pdf/pdf_function.hpp>

#include <odr/internal/pdf/pdf_object.hpp>
#include <odr/internal/pdf/pdf_object_parser.hpp>
#include <odr/internal/util/number_util.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <variant>

namespace odr::internal::pdf {

namespace {

double clamp(const double v, const double lo, const double hi) {
  return std::clamp(v, std::min(lo, hi), std::max(lo, hi));
}

/// Affine map of `x` from `[a0, a1]` onto `[b0, b1]` (ISO 32000-1's
/// "Interpolate" — used for the sampled-function encode/decode and the
/// stitching encode).
double interpolate(const double x, const double a0, const double a1,
                   const double b0, const double b1) {
  if (a1 == a0) {
    return b0;
  }
  return b0 + (x - a0) * (b1 - b0) / (a1 - a0);
}

// --- type 2: exponential interpolation (ISO 32000-1 7.10.3) ----------------

class ExponentialFunction final : public Function {
public:
  ExponentialFunction(std::vector<double> domain, std::vector<double> range,
                      std::vector<double> c0, std::vector<double> c1,
                      const double n)
      : Function(std::move(domain), std::move(range)), m_c0{std::move(c0)},
        m_c1{std::move(c1)}, m_n{n} {}

protected:
  std::vector<double> compute(const std::span<const double> in) const override {
    const double x = in.empty() ? 0.0 : in[0];
    const double xn = std::pow(x, m_n);
    const std::size_t count = m_c0.size();
    std::vector<double> out(count);
    for (std::size_t j = 0; j < count; ++j) {
      out[j] = m_c0[j] + xn * (m_c1[j] - m_c0[j]);
    }
    return out;
  }

private:
  std::vector<double> m_c0;
  std::vector<double> m_c1;
  double m_n;
};

// --- type 3: stitching (ISO 32000-1 7.10.4) --------------------------------

class StitchingFunction final : public Function {
public:
  StitchingFunction(std::vector<double> domain, std::vector<double> range,
                    std::vector<std::shared_ptr<Function>> functions,
                    std::vector<double> bounds, std::vector<double> encode)
      : Function(std::move(domain), std::move(range)),
        m_functions{std::move(functions)}, m_bounds{std::move(bounds)},
        m_encode{std::move(encode)} {}

protected:
  std::vector<double> compute(const std::span<const double> in) const override {
    const double d0 = m_domain[0];
    const double d1 = m_domain[1];
    const double x = in.empty() ? d0 : in[0];

    // Locate the subinterval: bounds[i-1] <= x < bounds[i].
    std::size_t k = 0;
    while (k < m_bounds.size() && x >= m_bounds[k]) {
      ++k;
    }
    const double lo = k == 0 ? d0 : m_bounds[k - 1];
    const double hi = k < m_bounds.size() ? m_bounds[k] : d1;
    const double encoded =
        interpolate(x, lo, hi, m_encode[2 * k], m_encode[2 * k + 1]);
    return m_functions[k]->eval({encoded});
  }

private:
  std::vector<std::shared_ptr<Function>> m_functions;
  std::vector<double> m_bounds;
  std::vector<double> m_encode;
};

// --- type 0: sampled (ISO 32000-1 7.10.2) ----------------------------------

class SampledFunction final : public Function {
public:
  SampledFunction(std::vector<double> domain, std::vector<double> range,
                  std::vector<std::size_t> size,
                  const std::int32_t bits_per_sample,
                  std::vector<double> encode, std::vector<double> decode,
                  std::string samples)
      : Function(std::move(domain), std::move(range)), m_size{std::move(size)},
        m_bits{bits_per_sample}, m_encode{std::move(encode)},
        m_decode{std::move(decode)}, m_samples{std::move(samples)} {}

protected:
  std::vector<double> compute(const std::span<const double> in) const override {
    const std::size_t m = m_size.size();
    const std::size_t n = output_arity();
    if (m == 0 || n == 0) {
      return {};
    }

    // Encode each input coordinate into its sample grid [0, size_i - 1].
    std::vector<double> e(m);
    for (std::size_t i = 0; i < m; ++i) {
      const std::size_t d = 2 * i;
      const double x = i < in.size() ? in[i] : m_domain[d];
      const double enc = interpolate(x, m_domain[d], m_domain[d + 1],
                                     m_encode[d], m_encode[d + 1]);
      e[i] = clamp(enc, 0.0, static_cast<double>(m_size[i]) - 1.0);
    }

    // Multilinear interpolation across the 2^m surrounding grid corners.
    std::vector<std::size_t> base(m);
    std::vector<double> frac(m);
    for (std::size_t i = 0; i < m; ++i) {
      const std::size_t floor_i =
          std::min(static_cast<std::size_t>(std::floor(e[i])), m_size[i] - 1);
      base[i] = floor_i;
      frac[i] = e[i] - static_cast<double>(floor_i);
    }

    std::vector<double> out(n, 0.0);
    const std::size_t corners = std::size_t{1} << m;
    for (std::size_t c = 0; c < corners; ++c) {
      double weight = 1.0;
      std::vector<std::size_t> coord(m);
      for (std::size_t i = 0; i < m; ++i) {
        const bool high = ((c >> i) & 1U) != 0;
        std::size_t ci = base[i] + (high ? 1 : 0);
        ci = std::min(ci, m_size[i] - 1);
        coord[i] = ci;
        weight *= high ? frac[i] : (1.0 - frac[i]);
      }
      if (weight == 0.0) {
        continue;
      }
      const std::size_t index = sample_index(coord);
      for (std::size_t j = 0; j < n; ++j) {
        out[j] += weight * raw_sample(index * n + j);
      }
    }

    // Decode each output from [0, 2^bits - 1] onto its Decode range.
    const double max_value = std::ldexp(1.0, m_bits) - 1.0;
    for (std::size_t j = 0; j < n; ++j) {
      const std::size_t d = 2 * j;
      out[j] =
          interpolate(out[j], 0.0, max_value, m_decode[d], m_decode[d + 1]);
    }
    return out;
  }

private:
  [[nodiscard]] std::size_t
  sample_index(const std::vector<std::size_t> &coord) const {
    std::size_t index = 0;
    std::size_t stride = 1;
    for (std::size_t i = 0; i < coord.size(); ++i) {
      index += coord[i] * stride;
      stride *= m_size[i];
    }
    return index;
  }

  /// The `k`-th `m_bits`-wide unsigned sample, MSB-first (ISO 32000-1 7.10.2).
  [[nodiscard]] double raw_sample(const std::size_t k) const {
    const std::size_t bit_offset = k * static_cast<std::size_t>(m_bits);
    std::uint64_t value = 0;
    for (std::int32_t i = 0; i < m_bits; ++i) {
      const std::size_t bit = bit_offset + static_cast<std::size_t>(i);
      const std::size_t byte = bit / 8;
      std::uint64_t sample_bit = 0;
      if (byte < m_samples.size()) {
        const auto b = static_cast<std::uint8_t>(m_samples[byte]);
        sample_bit = (b >> (7 - bit % 8)) & 1U;
      }
      value = (value << 1) | sample_bit;
    }
    return static_cast<double>(value);
  }

  std::vector<std::size_t> m_size;
  std::int32_t m_bits;
  std::vector<double> m_encode;
  std::vector<double> m_decode;
  std::string m_samples;
};

// --- type 4: PostScript calculator (ISO 32000-1 7.10.5) --------------------

/// The integer operand of `idiv`/`mod`/the bitwise operators. Every type-4
/// value is held as a `double`, and converting one outside the destination
/// range is undefined behaviour, so saturate to the 32-bit signed range a
/// PostScript integer occupies and map NaN to zero.
std::int32_t to_int32(const double v) {
  constexpr auto lowest =
      static_cast<double>(std::numeric_limits<std::int32_t>::min());
  constexpr auto highest =
      static_cast<double>(std::numeric_limits<std::int32_t>::max());
  if (std::isnan(v)) {
    return 0;
  }
  return static_cast<std::int32_t>(std::clamp(v, lowest, highest));
}

/// Whether `x / y` (and `x % y`) is undefined in C++: a zero divisor, or the
/// non-representable `INT32_MIN / -1`.
bool is_undefined_division(const std::int32_t x, const std::int32_t y) {
  return y == 0 || (y == -1 && x == std::numeric_limits<std::int32_t>::min());
}

/// One token of a type-4 program: a literal number, an operator name, or a
/// nested `{ ... }` procedure block (used by `if`/`ifelse`).
struct PostScriptItem {
  enum class Kind { number, op, block };
  Kind kind{Kind::op};
  double number{0};
  std::string op;
  std::vector<PostScriptItem> block;
};

class PostScriptFunction final : public Function {
public:
  PostScriptFunction(std::vector<double> domain, std::vector<double> range,
                     std::vector<PostScriptItem> program)
      : Function(std::move(domain), std::move(range)),
        m_program{std::move(program)} {}

protected:
  std::vector<double> compute(const std::span<const double> in) const override {
    std::vector<Item> stack;
    stack.reserve(in.size());
    for (const double x : in) {
      stack.emplace_back(x);
    }
    try {
      std::size_t remaining = 100000;
      run(m_program, stack, remaining, 0);
      const std::size_t n = output_arity();
      if (stack.size() != n) {
        throw std::runtime_error("invalid calculator result count");
      }
      std::vector<double> out(n);
      for (std::size_t j = n; j-- > 0;) {
        out[j] = pop_number(stack);
        if (!std::isfinite(out[j])) {
          throw std::runtime_error("non-finite calculator result");
        }
      }
      return out;
    } catch (const std::exception &) {
      return std::vector<double>(output_arity(), 0.0);
    }
  }

private:
  using Item = std::variant<double, const std::vector<PostScriptItem> *>;

  static double pop_number(std::vector<Item> &s) {
    if (s.empty()) {
      throw std::runtime_error("stack underflow");
    }
    const double v = std::get<double>(s.back());
    s.pop_back();
    return v;
  }

  static const std::vector<PostScriptItem> *pop_block(std::vector<Item> &s) {
    if (s.empty()) {
      throw std::runtime_error("stack underflow");
    }
    const auto *b = std::get<const std::vector<PostScriptItem> *>(s.back());
    s.pop_back();
    return b;
  }

  static std::int32_t pop_integer(std::vector<Item> &s) {
    const double value = pop_number(s);
    if (!std::isfinite(value) || std::trunc(value) != value ||
        value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
      throw std::runtime_error("invalid calculator integer");
    }
    return static_cast<std::int32_t>(value);
  }

  static std::size_t pop_count(std::vector<Item> &s) {
    const std::int32_t count = pop_integer(s);
    if (count < 0 || static_cast<std::size_t>(count) > s.size()) {
      throw std::runtime_error("invalid calculator stack count");
    }
    return static_cast<std::size_t>(count);
  }

  static constexpr std::size_t max_stack = 4096;
  static constexpr double deg = 180.0 / std::numbers::pi;

  static void run(const std::vector<PostScriptItem> &program,
                  std::vector<Item> &s, std::size_t &remaining,
                  const std::size_t depth) {
    if (depth >= 64 || s.size() > max_stack) {
      throw std::runtime_error("calculator execution limit exceeded");
    }
    for (const PostScriptItem &item : program) {
      if (remaining == 0) {
        throw std::runtime_error("calculator execution limit exceeded");
      }
      --remaining;
      switch (item.kind) {
      case PostScriptItem::Kind::number:
        s.emplace_back(item.number);
        break;
      case PostScriptItem::Kind::block:
        s.emplace_back(&item.block);
        break;
      case PostScriptItem::Kind::op:
        run_op(item.op, s, remaining, depth);
        break;
      }
      if (s.size() > max_stack) {
        throw std::runtime_error("calculator stack limit exceeded");
      }
    }
  }

  static void run_op(const std::string &op, std::vector<Item> &s,
                     std::size_t &remaining, const std::size_t depth) {
    const auto unary = [&](double (*f)(double)) {
      s.emplace_back(f(pop_number(s)));
    };
    const auto binary = [&](double (*f)(double, double)) {
      const double b = pop_number(s);
      const double a = pop_number(s);
      s.emplace_back(f(a, b));
    };

    if (op == "add") {
      binary([](double a, double b) { return a + b; });
    } else if (op == "sub") {
      binary([](double a, double b) { return a - b; });
    } else if (op == "mul") {
      binary([](double a, double b) { return a * b; });
    } else if (op == "div") {
      binary([](double a, double b) { return b == 0 ? 0.0 : a / b; });
    } else if (op == "idiv") {
      binary([](const double a, const double b) {
        const std::int32_t x = to_int32(a);
        const std::int32_t y = to_int32(b);
        // A zero divisor already yielded 0 before; INT32_MIN / -1 joins it.
        if (is_undefined_division(x, y)) {
          return 0.0;
        }
        // NOLINTNEXTLINE(bugprone-integer-division): idiv is integer division
        return static_cast<double>(x / y);
      });
    } else if (op == "mod") {
      binary([](const double a, const double b) {
        const std::int32_t x = to_int32(a);
        const std::int32_t y = to_int32(b);
        return is_undefined_division(x, y) ? 0.0 : static_cast<double>(x % y);
      });
    } else if (op == "neg") {
      unary([](double a) { return -a; });
    } else if (op == "abs") {
      unary([](double a) { return std::abs(a); });
    } else if (op == "sqrt") {
      unary([](double a) { return std::sqrt(std::max(0.0, a)); });
    } else if (op == "sin") {
      unary([](double a) { return std::sin(a / deg); });
    } else if (op == "cos") {
      unary([](double a) { return std::cos(a / deg); });
    } else if (op == "atan") {
      const double den = pop_number(s);
      const double num = pop_number(s);
      double a = std::atan2(num, den) * deg;
      if (a < 0) {
        a += 360;
      }
      s.emplace_back(a);
    } else if (op == "exp") {
      binary([](double a, double b) { return std::pow(a, b); });
    } else if (op == "ln") {
      unary([](double a) { return std::log(std::max(1e-12, a)); });
    } else if (op == "log") {
      unary([](double a) { return std::log10(std::max(1e-12, a)); });
    } else if (op == "cvi" || op == "truncate") {
      unary([](double a) { return std::trunc(a); });
    } else if (op == "cvr") {
      // no-op: every value is already real
    } else if (op == "floor") {
      unary([](double a) { return std::floor(a); });
    } else if (op == "ceiling") {
      unary([](double a) { return std::ceil(a); });
    } else if (op == "round") {
      unary([](double a) { return std::round(a); });
    } else if (op == "eq") {
      binary([](double a, double b) { return a == b ? 1.0 : 0.0; });
    } else if (op == "ne") {
      binary([](double a, double b) { return a != b ? 1.0 : 0.0; });
    } else if (op == "gt") {
      binary([](double a, double b) { return a > b ? 1.0 : 0.0; });
    } else if (op == "ge") {
      binary([](double a, double b) { return a >= b ? 1.0 : 0.0; });
    } else if (op == "lt") {
      binary([](double a, double b) { return a < b ? 1.0 : 0.0; });
    } else if (op == "le") {
      binary([](double a, double b) { return a <= b ? 1.0 : 0.0; });
    } else if (op == "and") {
      bitwise_or_logical(
          s, [](std::int32_t a, std::int32_t b) { return a & b; },
          [](bool a, bool b) { return a && b; });
    } else if (op == "or") {
      bitwise_or_logical(
          s, [](std::int32_t a, std::int32_t b) { return a | b; },
          [](bool a, bool b) { return a || b; });
    } else if (op == "xor") {
      bitwise_or_logical(
          s, [](std::int32_t a, std::int32_t b) { return a ^ b; },
          [](bool a, bool b) { return a != b; });
    } else if (op == "not") {
      const double a = pop_number(s);
      if (a == 0.0 || a == 1.0) {
        s.emplace_back(a == 0.0 ? 1.0 : 0.0);
      } else {
        s.emplace_back(static_cast<double>(~to_int32(a)));
      }
    } else if (op == "bitshift") {
      const double shift = pop_number(s);
      const std::int32_t value = to_int32(pop_number(s));
      // Shifting a 32-bit value by 32 or more is undefined in C++; PostScript
      // shifts every bit out. (The comparison also catches a NaN shift.)
      const double magnitude = std::abs(shift);
      const std::int32_t by =
          magnitude < 32.0 ? static_cast<std::int32_t>(magnitude) : 32;
      s.emplace_back(static_cast<double>(
          by == 32 ? 0 : (shift >= 0 ? value << by : value >> by)));
    } else if (op == "true") {
      s.emplace_back(1.0);
    } else if (op == "false") {
      s.emplace_back(0.0);
    } else if (op == "pop") {
      pop_number(s);
    } else if (op == "exch") {
      const Item b = s.at(s.size() - 1);
      const Item a = s.at(s.size() - 2);
      s[s.size() - 1] = a;
      s[s.size() - 2] = b;
    } else if (op == "dup") {
      s.push_back(s.at(s.size() - 1));
    } else if (op == "copy") {
      const std::size_t count = pop_count(s);
      if (count > max_stack - s.size()) {
        throw std::runtime_error("calculator stack limit exceeded");
      }
      const std::size_t start = s.size() - count;
      for (std::size_t i = 0; i < count; ++i) {
        s.push_back(s[start + i]);
      }
    } else if (op == "index") {
      const std::size_t i = pop_count(s);
      s.push_back(s.at(s.size() - 1 - i));
    } else if (op == "roll") {
      const std::int32_t j = pop_integer(s);
      const auto count = static_cast<std::int32_t>(pop_count(s));
      if (count > 0) {
        const auto first = s.end() - count;
        const std::int32_t remainder = j % count;
        const std::int32_t shift =
            remainder < 0 ? remainder + count : remainder;
        std::rotate(first, s.end() - shift, s.end());
      }
    } else if (op == "if") {
      const auto *proc = pop_block(s);
      const double cond = pop_number(s);
      if (cond != 0.0) {
        run(*proc, s, remaining, depth + 1);
      }
    } else if (op == "ifelse") {
      const auto *proc2 = pop_block(s);
      const auto *proc1 = pop_block(s);
      const double cond = pop_number(s);
      run(cond != 0.0 ? *proc1 : *proc2, s, remaining, depth + 1);
    } else {
      throw std::runtime_error("unknown PostScript operator: " + op);
    }
  }

  template <typename IntOp, typename BoolOp>
  static void bitwise_or_logical(std::vector<Item> &s, IntOp int_op,
                                 BoolOp bool_op) {
    const double b = pop_number(s);
    const double a = pop_number(s);
    const bool boolean = (a == 0.0 || a == 1.0) && (b == 0.0 || b == 1.0);
    if (boolean) {
      s.emplace_back(bool_op(a != 0.0, b != 0.0) ? 1.0 : 0.0);
    } else {
      s.emplace_back(static_cast<double>(int_op(to_int32(a), to_int32(b))));
    }
  }

  std::vector<PostScriptItem> m_program;
};

/// Tokenizes a type-4 program (ISO 32000-1, 7.10.5.1) into a nested item tree.
/// `pos` advances past the consumed text. Nothing if the braces do not balance,
/// nest too deep or a number token is malformed.
std::optional<std::vector<PostScriptItem>>
parse_postscript(const std::string &text, std::size_t &pos,
                 const std::size_t depth = 0) {
  if (depth >= 64) {
    return std::nullopt;
  }
  std::vector<PostScriptItem> items;
  while (pos < text.size()) {
    const char c = text[pos];
    if (ObjectParser::is_whitespace(c)) {
      ++pos;
    } else if (c == '%') {
      pos = text.find_first_of("\r\n", pos);
      if (pos == std::string::npos) {
        pos = text.size();
      }
    } else if (c == '{') {
      ++pos;
      auto block = parse_postscript(text, pos, depth + 1);
      if (!block.has_value()) {
        return std::nullopt;
      }
      PostScriptItem item;
      item.kind = PostScriptItem::Kind::block;
      item.block = std::move(*block);
      items.push_back(std::move(item));
    } else if (c == '}') {
      ++pos;
      return depth == 0 ? std::nullopt : std::optional(std::move(items));
    } else {
      const std::size_t start = pos;
      while (pos < text.size() && !ObjectParser::is_whitespace(text[pos]) &&
             text[pos] != '%' && text[pos] != '{' && text[pos] != '}') {
        ++pos;
      }
      const std::string_view token(text.data() + start, pos - start);
      PostScriptItem item;
      if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
        const auto number = util::number::parse(token);
        if (!number.has_value() || !std::isfinite(*number)) {
          return std::nullopt;
        }
        item.kind = PostScriptItem::Kind::number;
        item.number = *number;
      } else {
        item.kind = PostScriptItem::Kind::op;
        item.op = token;
      }
      items.push_back(std::move(item));
    }
  }
  return depth == 0 ? std::optional(std::move(items)) : std::nullopt;
}

} // namespace

std::vector<double> Function::eval(std::vector<double> in) const {
  const std::size_t m = input_arity();
  in.resize(m, 0.0);
  for (std::size_t i = 0; i < m; ++i) {
    const std::size_t d = 2 * i;
    in[i] = clamp(in[i], m_domain[d], m_domain[d + 1]);
  }
  std::vector<double> out = compute(in);
  const std::size_t n = output_arity();
  if (n != 0) {
    out.resize(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
      const std::size_t d = 2 * j;
      out[j] = clamp(out[j], m_range[d], m_range[d + 1]);
    }
  }
  return out;
}

namespace {

bool valid_intervals(const std::vector<double> &values) {
  if (values.size() % 2 != 0) {
    return false;
  }
  for (std::size_t i = 0; i < values.size(); i += 2) {
    if (!std::isfinite(values[i]) || !std::isfinite(values[i + 1]) ||
        values[i] > values[i + 1]) {
      return false;
    }
  }
  return true;
}

std::shared_ptr<Function> parse_function_impl(const Object &object,
                                              const FunctionContext &context,
                                              const std::size_t depth) {
  if (depth >= 64) {
    return nullptr;
  }
  const Object resolved = context.resolve(object);
  if (!resolved.is_dictionary()) {
    return nullptr;
  }
  const Dictionary &dict = resolved.as_dictionary();
  if (!dict.get("FunctionType").is_integer()) {
    return nullptr;
  }
  const std::int64_t type = dict.get("FunctionType").as_integer();

  std::vector<double> domain = dict.get("Domain").as_reals();
  std::vector<double> range = dict.get("Range").as_reals();
  if (!valid_intervals(domain) || !valid_intervals(range)) {
    return nullptr;
  }
  if (domain.empty() && (type == 2 || type == 3)) {
    domain = {0.0, 1.0};
  }

  switch (type) {
  case 2: {
    std::vector<double> c0 = dict.get("C0").as_reals();
    std::vector<double> c1 = dict.get("C1").as_reals();
    if (c0.empty()) {
      c0 = {0.0};
    }
    if (c1.empty()) {
      c1 = {1.0};
    }
    const double n = dict.get("N").as_real();
    // `/C0` and `/C1` must be equally long; a malformed file may disagree.
    const std::size_t outputs = std::min(c0.size(), c1.size());
    c0.resize(outputs);
    c1.resize(outputs);
    if (domain.size() != 2 ||
        (!range.empty() && range.size() / 2 != c0.size()) ||
        !std::isfinite(n)) {
      return nullptr;
    }
    return std::make_shared<ExponentialFunction>(
        std::move(domain), std::move(range), std::move(c0), std::move(c1), n);
  }
  case 3: {
    std::vector<std::shared_ptr<Function>> functions;
    if (const Object &fns = dict.get("Functions"); fns.is_array()) {
      for (const Object &fn : fns.as_array()) {
        auto child = parse_function_impl(fn, context, depth + 1);
        if (child == nullptr || child->input_arity() != 1) {
          return nullptr;
        }
        functions.push_back(std::move(child));
      }
    }
    std::vector<double> bounds = dict.get("Bounds").as_reals();
    std::vector<double> encode = dict.get("Encode").as_reals();
    if (domain.size() != 2 || functions.empty() ||
        bounds.size() != functions.size() - 1 ||
        encode.size() != 2 * functions.size()) {
      return nullptr;
    }
    // an empty subdomain evaluates to its encode start, so equal bounds pass
    double previous = domain.front();
    for (const double bound : bounds) {
      if (!std::isfinite(bound) || bound < previous || bound > domain.back()) {
        return nullptr;
      }
      previous = bound;
    }
    if (!std::ranges::all_of(encode,
                             [](const double v) { return std::isfinite(v); })) {
      return nullptr;
    }
    return std::make_shared<StitchingFunction>(
        std::move(domain), std::move(range), std::move(functions),
        std::move(bounds), std::move(encode));
  }
  case 0: {
    std::vector<std::size_t> size;
    if (const Object &s = dict.get("Size"); s.is_array()) {
      for (const Object &item : s.as_array()) {
        const std::int64_t dimension = item.as_integer();
        if (dimension <= 0 || static_cast<std::uint64_t>(dimension) >
                                  std::numeric_limits<std::size_t>::max()) {
          return nullptr;
        }
        size.push_back(static_cast<std::size_t>(dimension));
      }
    }
    const std::int64_t bits = dict.get("BitsPerSample").as_integer();
    // ISO 32000-1 Table 39.
    if (bits != 1 && bits != 2 && bits != 4 && bits != 8 && bits != 12 &&
        bits != 16 && bits != 24 && bits != 32) {
      return nullptr;
    }
    std::vector<double> encode = dict.get("Encode").as_reals();
    if (encode.empty()) {
      for (const std::size_t dim : size) {
        encode.push_back(0.0);
        encode.push_back(static_cast<double>(dim) - 1.0);
      }
    }
    std::vector<double> decode = dict.get("Decode").as_reals();
    if (decode.empty()) {
      decode = range;
    }
    std::string samples = context.load_stream(object);
    if (domain.empty()) {
      domain.assign(2 * size.size(), 0.0);
      for (std::size_t i = 1; i < domain.size(); i += 2) {
        domain[i] = 1.0;
      }
    }
    constexpr std::size_t max_inputs = 8;
    if (size.empty() || size.size() > max_inputs || range.empty() ||
        domain.size() != 2 * size.size() || encode.size() != domain.size() ||
        decode.size() != range.size() ||
        !std::ranges::all_of(encode,
                             [](const double v) { return std::isfinite(v); }) ||
        !std::ranges::all_of(decode,
                             [](const double v) { return std::isfinite(v); })) {
      return nullptr;
    }
    std::size_t sample_bits = range.size() / 2;
    for (const std::size_t dimension : size) {
      if (sample_bits > std::numeric_limits<std::size_t>::max() / dimension) {
        return nullptr;
      }
      sample_bits *= dimension;
    }
    if (sample_bits > std::numeric_limits<std::size_t>::max() /
                          static_cast<std::size_t>(bits)) {
      return nullptr;
    }
    sample_bits *= static_cast<std::size_t>(bits);
    if (sample_bits / 8 + (sample_bits % 8 != 0) > samples.size()) {
      return nullptr;
    }
    return std::make_shared<SampledFunction>(
        std::move(domain), std::move(range), std::move(size),
        static_cast<std::int32_t>(bits), std::move(encode), std::move(decode),
        std::move(samples));
  }
  case 4: {
    const std::string program = context.load_stream(object);
    std::size_t pos = 0;
    auto items = parse_postscript(program, pos);
    if (!items.has_value() || items->size() != 1 ||
        items->front().kind != PostScriptItem::Kind::block || domain.empty() ||
        range.empty()) {
      return nullptr;
    }
    return std::make_shared<PostScriptFunction>(
        std::move(domain), std::move(range), std::move(items->front().block));
  }
  default:
    return nullptr;
  }
}

} // namespace
} // namespace odr::internal::pdf

namespace odr::internal {

std::shared_ptr<pdf::Function>
pdf::parse_function(const Object &object, const FunctionContext &context) {
  return parse_function_impl(object, context, 0);
}

} // namespace odr::internal
