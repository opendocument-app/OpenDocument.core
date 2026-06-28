#include <odr/internal/pdf/pdf_function.hpp>

#include <odr/internal/pdf/pdf_object.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <variant>

namespace odr::internal::pdf {

namespace {

/// Read an array of numbers from a dictionary entry, or `{}` when absent.
std::vector<double> read_numbers(const Dictionary &dict, const char *key) {
  std::vector<double> result;
  if (const Object &value = dict.get(key); value.is_array()) {
    const Array &array = value.as_array();
    result.reserve(array.size());
    for (const Object &item : array) {
      result.push_back(item.as_real());
    }
  }
  return result;
}

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
  std::vector<double> compute(const std::vector<double> &in) const override {
    const double x = in.empty() ? 0.0 : in[0];
    const double xn = std::pow(x, m_n);
    std::vector<double> out(m_c0.size());
    for (std::size_t j = 0; j < m_c0.size(); ++j) {
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
  std::vector<double> compute(const std::vector<double> &in) const override {
    if (m_functions.empty()) {
      return {};
    }
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
    const double e0 = m_encode[2 * k];
    const double e1 = m_encode[2 * k + 1];
    const double encoded = interpolate(x, lo, hi, e0, e1);

    if (m_functions[k] == nullptr) {
      return {};
    }
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
  std::vector<double> compute(const std::vector<double> &in) const override {
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
  std::vector<double> compute(const std::vector<double> &in) const override {
    std::vector<Item> stack;
    stack.reserve(in.size());
    for (const double x : in) {
      stack.emplace_back(x);
    }
    try {
      run(m_program, stack);
    } catch (const std::exception &) {
      return std::vector<double>(output_arity(), 0.0);
    }

    const std::size_t n = output_arity();
    std::vector<double> out(n, 0.0);
    // The function leaves n results on the stack, the last output on top.
    for (std::size_t j = n; j-- > 0 && !stack.empty();) {
      out[j] = std::get<double>(stack.back());
      stack.pop_back();
    }
    return out;
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

  static constexpr double deg = 180.0 / std::numbers::pi;

  static void run(const std::vector<PostScriptItem> &program,
                  std::vector<Item> &s) {
    for (const PostScriptItem &item : program) {
      switch (item.kind) {
      case PostScriptItem::Kind::number:
        s.emplace_back(item.number);
        break;
      case PostScriptItem::Kind::block:
        s.emplace_back(&item.block);
        break;
      case PostScriptItem::Kind::op:
        run_op(item.op, s);
        break;
      }
    }
  }

  static void run_op(const std::string &op, std::vector<Item> &s) {
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
      binary([](double a, double b) {
        if (b == 0) {
          return 0.0;
        }
        // NOLINTNEXTLINE(bugprone-integer-division): idiv is integer division
        return static_cast<double>(static_cast<std::int32_t>(a) /
                                   static_cast<std::int32_t>(b));
      });
    } else if (op == "mod") {
      binary([](double a, double b) {
        return b == 0 ? 0.0
                      : static_cast<double>(static_cast<std::int32_t>(a) %
                                            static_cast<std::int32_t>(b));
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
        s.emplace_back(static_cast<double>(~static_cast<std::int32_t>(a)));
      }
    } else if (op == "bitshift") {
      const auto shift = static_cast<std::int32_t>(pop_number(s));
      const auto value = static_cast<std::int32_t>(pop_number(s));
      s.emplace_back(
          static_cast<double>(shift >= 0 ? value << shift : value >> -shift));
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
      s.push_back(s.back());
    } else if (op == "copy") {
      const auto count = static_cast<std::size_t>(pop_number(s));
      const std::size_t start = s.size() - count;
      for (std::size_t i = 0; i < count; ++i) {
        s.push_back(s[start + i]);
      }
    } else if (op == "index") {
      const auto i = static_cast<std::size_t>(pop_number(s));
      s.push_back(s.at(s.size() - 1 - i));
    } else if (op == "roll") {
      const auto j = static_cast<std::int32_t>(pop_number(s));
      const auto count = static_cast<std::int32_t>(pop_number(s));
      if (count > 0) {
        const auto first = s.end() - count;
        const std::int32_t shift = ((j % count) + count) % count;
        std::rotate(first, s.end() - shift, s.end());
      }
    } else if (op == "if") {
      const auto *proc = pop_block(s);
      const double cond = pop_number(s);
      if (cond != 0.0) {
        run(*proc, s);
      }
    } else if (op == "ifelse") {
      const auto *proc2 = pop_block(s);
      const auto *proc1 = pop_block(s);
      const double cond = pop_number(s);
      run(cond != 0.0 ? *proc1 : *proc2, s);
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
      s.emplace_back(static_cast<double>(
          int_op(static_cast<std::int32_t>(a), static_cast<std::int32_t>(b))));
    }
  }

  std::vector<PostScriptItem> m_program;
};

/// Tokenize a type-4 program body into a nested item tree. `pos` advances past
/// the consumed text; a `{` recurses and the matching `}` returns.
std::vector<PostScriptItem> parse_postscript(const std::string &text,
                                             std::size_t &pos) {
  std::vector<PostScriptItem> items;
  while (pos < text.size()) {
    const char c = text[pos];
    if (std::isspace(static_cast<unsigned char>(c)) != 0) {
      ++pos;
    } else if (c == '{') {
      ++pos;
      PostScriptItem item;
      item.kind = PostScriptItem::Kind::block;
      item.block = parse_postscript(text, pos);
      items.push_back(std::move(item));
    } else if (c == '}') {
      ++pos;
      return items;
    } else if ((std::isdigit(static_cast<unsigned char>(c)) != 0) || c == '-' ||
               c == '+' || c == '.') {
      std::size_t end = pos;
      while (end < text.size() &&
             (std::isdigit(static_cast<unsigned char>(text[end])) != 0 ||
              text[end] == '-' || text[end] == '+' || text[end] == '.' ||
              text[end] == 'e' || text[end] == 'E')) {
        ++end;
      }
      PostScriptItem item;
      item.kind = PostScriptItem::Kind::number;
      item.number = std::stod(text.substr(pos, end - pos));
      items.push_back(std::move(item));
      pos = end;
    } else {
      std::size_t end = pos;
      while (end < text.size() &&
             std::isalpha(static_cast<unsigned char>(text[end])) != 0) {
        ++end;
      }
      if (end == pos) {
        ++pos; // skip an unexpected character
        continue;
      }
      PostScriptItem item;
      item.kind = PostScriptItem::Kind::op;
      item.op = text.substr(pos, end - pos);
      items.push_back(std::move(item));
      pos = end;
    }
  }
  return items;
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

} // namespace odr::internal::pdf

namespace odr::internal {

std::shared_ptr<pdf::Function>
pdf::parse_function(const Object &object, const FunctionContext &context) {
  const Object resolved = context.resolve(object);
  if (!resolved.is_dictionary()) {
    return nullptr;
  }
  const Dictionary &dict = resolved.as_dictionary();
  if (!dict.get("FunctionType").is_integer()) {
    return nullptr;
  }
  const std::int32_t type =
      static_cast<std::int32_t>(dict.get("FunctionType").as_integer());

  std::vector<double> domain = read_numbers(dict, "Domain");
  std::vector<double> range = read_numbers(dict, "Range");

  switch (type) {
  case 2: {
    std::vector<double> c0 = read_numbers(dict, "C0");
    std::vector<double> c1 = read_numbers(dict, "C1");
    if (c0.empty()) {
      c0 = {0.0};
    }
    if (c1.empty()) {
      c1 = {1.0};
    }
    const double n = dict.get("N").as_real();
    if (domain.empty()) {
      domain = {0.0, 1.0};
    }
    return std::make_shared<ExponentialFunction>(
        std::move(domain), std::move(range), std::move(c0), std::move(c1), n);
  }
  case 3: {
    std::vector<std::shared_ptr<Function>> functions;
    if (const Object &fns = dict.get("Functions"); fns.is_array()) {
      for (const Object &fn : fns.as_array()) {
        functions.push_back(parse_function(fn, context));
      }
    }
    std::vector<double> bounds = read_numbers(dict, "Bounds");
    std::vector<double> encode = read_numbers(dict, "Encode");
    if (domain.empty()) {
      domain = {0.0, 1.0};
    }
    return std::make_shared<StitchingFunction>(
        std::move(domain), std::move(range), std::move(functions),
        std::move(bounds), std::move(encode));
  }
  case 0: {
    std::vector<std::size_t> size;
    if (const Object &s = dict.get("Size"); s.is_array()) {
      for (const Object &item : s.as_array()) {
        size.push_back(static_cast<std::size_t>(item.as_integer()));
      }
    }
    const std::int32_t bits =
        static_cast<std::int32_t>(dict.get("BitsPerSample").as_integer());
    std::vector<double> encode = read_numbers(dict, "Encode");
    if (encode.empty()) {
      for (const std::size_t dim : size) {
        encode.push_back(0.0);
        encode.push_back(static_cast<double>(dim) - 1.0);
      }
    }
    std::vector<double> decode = read_numbers(dict, "Decode");
    if (decode.empty()) {
      decode = range;
    }
    std::string samples = context.load_stream(object);
    if (size.empty() || bits == 0 || range.empty()) {
      return nullptr;
    }
    return std::make_shared<SampledFunction>(
        std::move(domain), std::move(range), std::move(size), bits,
        std::move(encode), std::move(decode), std::move(samples));
  }
  case 4: {
    const std::string program = context.load_stream(object);
    std::size_t pos = 0;
    std::vector<PostScriptItem> items = parse_postscript(program, pos);
    // Unwrap the outer `{ ... }` block so the program runs at top level. Move
    // the inner block out to a local first: assigning it back into `items`
    // directly would free the vector the source still lives in.
    if (items.size() == 1 &&
        items.front().kind == PostScriptItem::Kind::block) {
      std::vector<PostScriptItem> body = std::move(items.front().block);
      items = std::move(body);
    }
    if (domain.empty() || range.empty()) {
      return nullptr;
    }
    return std::make_shared<PostScriptFunction>(
        std::move(domain), std::move(range), std::move(items));
  }
  default:
    return nullptr;
  }
}

} // namespace odr::internal
