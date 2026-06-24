#include <odr/internal/font/type1_charstring.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace odr::internal::font::type1 {

namespace {

// Type1 charstring operators (single byte; 12 = escape to a two-byte op).
enum T1 : std::int32_t {
  t1_hstem = 1,
  t1_vstem = 3,
  t1_vmoveto = 4,
  t1_rlineto = 5,
  t1_hlineto = 6,
  t1_vlineto = 7,
  t1_rrcurveto = 8,
  t1_closepath = 9,
  t1_callsubr = 10,
  t1_return = 11,
  t1_hsbw = 13,
  t1_endchar = 14,
  t1_rmoveto = 21,
  t1_hmoveto = 22,
  t1_vhcurveto = 30,
  t1_hvcurveto = 31,
  t1_dotsection = 1200,
  t1_vstem3 = 1201,
  t1_hstem3 = 1202,
  t1_seac = 1206,
  t1_sbw = 1207,
  t1_div = 1212,
  t1_callothersubr = 1216,
  t1_pop = 1217,
  t1_setcurrentpoint = 1233,
};

/// Encode an integer operand in the Type2 charstring number forms.
void emit_int(std::string &out, const std::int32_t v) {
  if (v >= -107 && v <= 107) {
    out += static_cast<char>(v + 139);
  } else if (v >= 108 && v <= 1131) {
    const std::int32_t u = v - 108;
    out += static_cast<char>((u >> 8) + 247);
    out += static_cast<char>(u & 0xff);
  } else if (v >= -1131 && v <= -108) {
    const std::int32_t u = -v - 108;
    out += static_cast<char>((u >> 8) + 251);
    out += static_cast<char>(u & 0xff);
  } else {
    out += static_cast<char>(28); // shortint
    out += static_cast<char>((v >> 8) & 0xff);
    out += static_cast<char>(v & 0xff);
  }
}

/// Encode a (possibly fractional) operand: an integer form when whole and in
/// range, else the Type2 16.16 fixed form (`255 + int32`).
void emit_num(std::string &out, const double v) {
  if (v == std::floor(v) && v >= -32768 && v <= 32767) {
    emit_int(out, static_cast<std::int32_t>(v));
    return;
  }
  const auto fixed = static_cast<std::int32_t>(std::lround(v * 65536.0));
  out += static_cast<char>(255);
  out += static_cast<char>((fixed >> 24) & 0xff);
  out += static_cast<char>((fixed >> 16) & 0xff);
  out += static_cast<char>((fixed >> 8) & 0xff);
  out += static_cast<char>(fixed & 0xff);
}

/// The translation state machine. Walks the Type1 charstring (recursing through
/// `callsubr`), emitting a Type2 charstring.
class Translator {
public:
  explicit Translator(const std::vector<std::string> &subrs) : m_subrs(subrs) {}

  Type2Charstring run(const std::string_view charstring) {
    execute(charstring, 0);
    if (!m_ended) {
      m_out += static_cast<char>(t1_endchar);
    }
    return {std::move(m_out), m_width, m_has_width};
  }

private:
  // Emit the pending width (once) ahead of the first stem/move/endchar's
  // operands, as the Type2 width does. nominalWidthX is 0 in the built CFF, so
  // the width is the absolute advance.
  void emit_width() {
    if (m_width_pending) {
      emit_int(m_out, m_width);
      m_width_pending = false;
    }
  }

  void flush_stack() {
    for (const double v : m_stack) {
      emit_num(m_out, v);
    }
    m_stack.clear();
  }

  // Emit width + operands + a one-byte operator, clearing the stack.
  void emit_op(const std::int32_t op) {
    emit_width();
    flush_stack();
    m_out += static_cast<char>(op);
  }

  void execute(const std::string_view cs, const std::int32_t depth) {
    if (depth > 16 || m_ended) {
      return;
    }
    std::size_t p = 0;
    while (p < cs.size() && !m_ended) {
      const auto b = static_cast<std::uint8_t>(cs[p]);
      if (b >= 32) {
        // operand
        double value = 0.0;
        if (b <= 246) {
          value = static_cast<std::int32_t>(b) - 139;
          p += 1;
        } else if (b <= 250) {
          value = (static_cast<std::int32_t>(b) - 247) * 256 +
                  static_cast<std::uint8_t>(cs[p + 1]) + 108;
          p += 2;
        } else if (b <= 254) {
          value = -(static_cast<std::int32_t>(b) - 251) * 256 -
                  static_cast<std::uint8_t>(cs[p + 1]) - 108;
          p += 2;
        } else { // 255: Type1 32-bit integer
          value = static_cast<std::int32_t>(
              (static_cast<std::uint8_t>(cs[p + 1]) << 24) |
              (static_cast<std::uint8_t>(cs[p + 2]) << 16) |
              (static_cast<std::uint8_t>(cs[p + 3]) << 8) |
              static_cast<std::uint8_t>(cs[p + 4]));
          p += 5;
        }
        m_stack.push_back(value);
        continue;
      }
      std::int32_t op = b;
      ++p;
      if (b == 12) {
        op = 1200 + static_cast<std::uint8_t>(cs[p]);
        ++p;
      }
      handle(op, depth);
    }
  }

  void handle(const std::int32_t op, const std::int32_t depth) {
    switch (op) {
    case t1_hsbw:
      if (m_stack.size() >= 2) {
        m_sbx = m_stack[0];
        m_width = static_cast<std::int32_t>(m_stack[1]);
        m_has_width = true;
        m_width_pending = true;
        m_sbx_pending = true;
      }
      m_stack.clear();
      break;
    case t1_sbw:
      if (m_stack.size() >= 4) {
        m_sbx = m_stack[0];
        m_width = static_cast<std::int32_t>(m_stack[2]);
        m_has_width = true;
        m_width_pending = true;
        m_sbx_pending = true;
      }
      m_stack.clear();
      break;

    case t1_rmoveto:
      if (m_flex_active) {
        collect_flex_point();
      } else {
        if (m_sbx_pending && !m_stack.empty()) {
          m_stack[0] += m_sbx;
          m_sbx_pending = false;
        }
        emit_op(t1_rmoveto);
      }
      break;
    case t1_hmoveto:
      if (m_flex_active) {
        collect_flex_point();
      } else {
        // hmoveto has no y; the side bearing adds an x, so keep it hmoveto.
        if (m_sbx_pending && !m_stack.empty()) {
          m_stack[0] += m_sbx;
        }
        m_sbx_pending = false;
        emit_op(t1_hmoveto);
      }
      break;
    case t1_vmoveto:
      if (m_flex_active) {
        collect_flex_point();
      } else if (m_sbx_pending && m_sbx != 0.0) {
        // A side bearing adds an x offset, which vmoveto cannot carry: promote
        // to rmoveto(sbx, dy).
        const double dy = m_stack.empty() ? 0.0 : m_stack[0];
        m_stack = {m_sbx, dy};
        m_sbx_pending = false;
        emit_op(t1_rmoveto);
      } else {
        m_sbx_pending = false;
        emit_op(t1_vmoveto);
      }
      break;

    case t1_hstem:
    case t1_vstem:
    case t1_rlineto:
    case t1_hlineto:
    case t1_vlineto:
    case t1_rrcurveto:
    case t1_vhcurveto:
    case t1_hvcurveto:
      emit_op(op); // identical opcodes/semantics in Type2
      break;

    case t1_closepath:
    case t1_dotsection:
    case t1_vstem3:
    case t1_hstem3:
    case t1_setcurrentpoint:
      m_stack.clear(); // dropped (implicit / hints / no-op)
      break;

    case t1_div:
      if (m_stack.size() >= 2) {
        const double b = m_stack.back();
        m_stack.pop_back();
        const double a = m_stack.back();
        m_stack.pop_back();
        m_stack.push_back(b != 0.0 ? a / b : 0.0);
      }
      break;

    case t1_callsubr: {
      if (m_stack.empty()) {
        break;
      }
      const auto index = static_cast<std::int32_t>(m_stack.back());
      m_stack.pop_back();
      if (index >= 0 && index < static_cast<std::int32_t>(m_subrs.size())) {
        execute(m_subrs[index], depth + 1);
      }
      break;
    }
    case t1_return:
      break; // end of the current subr

    case t1_callothersubr:
      handle_othersubr();
      break;
    case t1_pop:
      // Push the value the matching callothersubr left on the PS stack.
      if (!m_ps_stack.empty()) {
        m_stack.push_back(m_ps_stack.back());
        m_ps_stack.pop_back();
      } else {
        m_stack.push_back(0.0);
      }
      break;

    case t1_seac:
      emit_seac();
      break;

    case t1_endchar:
      emit_op(t1_endchar);
      m_ended = true;
      break;

    default:
      m_stack.clear(); // unknown: skip
      break;
    }
  }

  // OtherSubr dispatch: flex (1 start / 2 add-point / 0 end) and hint
  // replacement (3). The Type1 convention is `arg1..argN N othersubr#
  // callothersubr`, so the operand stack top is the othersubr number, below it
  // the argument count, below that the arguments.
  void handle_othersubr() {
    if (m_stack.size() < 2) {
      m_stack.clear();
      return;
    }
    const auto othersubr = static_cast<std::int32_t>(m_stack.back());
    m_stack.pop_back();
    const auto argc = static_cast<std::int32_t>(m_stack.back());
    m_stack.pop_back();

    std::vector<double> args;
    for (std::int32_t i = 0; i < argc && !m_stack.empty(); ++i) {
      args.push_back(m_stack.back());
      m_stack.pop_back();
    }
    // args is reversed (top first); restore call order.
    std::reverse(args.begin(), args.end());

    switch (othersubr) {
    case 1: // flex start
      m_flex_active = true;
      m_flex_points.clear();
      break;
    case 2: // flex add-point marker (the rmoveto already collected the point)
      break;
    case 0: // flex end: emit two curves from the collected points
      emit_flex();
      m_flex_active = false;
      // OtherSubr 0 leaves the end x,y on the PS stack for `pop pop
      // setcurrentpoint`.
      if (args.size() >= 3) {
        m_ps_stack.push_back(args[2]); // y (popped second)
        m_ps_stack.push_back(args[1]); // x (popped first)
      }
      break;
    case 3: // hint replacement: result is the subr number, used by callsubr
      m_ps_stack.push_back(args.empty() ? 3.0 : args[0]);
      break;
    default:
      // Unknown OtherSubr: make the arguments available to subsequent pops.
      for (auto it = args.rbegin(); it != args.rend(); ++it) {
        m_ps_stack.push_back(*it);
      }
      break;
    }
  }

  // During flex the 7 points arrive as `dx dy rmoveto`; collect their deltas.
  void collect_flex_point() {
    const double dx = m_stack.size() >= 2 ? m_stack[m_stack.size() - 2] : 0.0;
    const double dy = m_stack.empty() ? 0.0 : m_stack.back();
    m_flex_points.push_back({dx, dy});
    m_stack.clear();
  }

  // Emit the flex as two rrcurvetos. Point 1 is the reference point; points
  // 2..7 are the two beziers. The first curve's leading delta folds in the
  // reference delta (point 2 relative to the pre-flex point = d1 + d2).
  void emit_flex() {
    if (m_flex_points.size() < 7) {
      m_flex_points.clear();
      return;
    }
    const auto &d = m_flex_points;
    emit_width();
    emit_num(m_out, d[1].x + d[0].x);
    emit_num(m_out, d[1].y + d[0].y);
    emit_num(m_out, d[2].x);
    emit_num(m_out, d[2].y);
    emit_num(m_out, d[3].x);
    emit_num(m_out, d[3].y);
    m_out += static_cast<char>(t1_rrcurveto);
    emit_num(m_out, d[4].x);
    emit_num(m_out, d[4].y);
    emit_num(m_out, d[5].x);
    emit_num(m_out, d[5].y);
    emit_num(m_out, d[6].x);
    emit_num(m_out, d[6].y);
    m_out += static_cast<char>(t1_rrcurveto);
    m_flex_points.clear();
  }

  // seac: asb adx ady bchar achar. Emit the Type2 deprecated endchar-seac form
  // `adx' ady bchar achar endchar`, adjusting adx for the accent side bearing.
  void emit_seac() {
    if (m_stack.size() >= 5) {
      const double asb = m_stack[0];
      const double adx = m_stack[1];
      const double ady = m_stack[2];
      const double bchar = m_stack[3];
      const double achar = m_stack[4];
      m_stack.clear();
      emit_width();
      emit_num(m_out, adx - asb + m_sbx);
      emit_num(m_out, ady);
      emit_num(m_out, bchar);
      emit_num(m_out, achar);
      m_out += static_cast<char>(t1_endchar);
    }
    m_ended = true;
  }

  struct Point {
    double x;
    double y;
  };

  const std::vector<std::string> &m_subrs;
  std::string m_out;
  std::vector<double> m_stack;
  std::vector<double> m_ps_stack;

  std::int32_t m_width{};
  bool m_has_width{};
  bool m_width_pending{};
  double m_sbx{};
  bool m_sbx_pending{};
  bool m_ended{};

  bool m_flex_active{};
  std::vector<Point> m_flex_points;
};

} // namespace

} // namespace odr::internal::font::type1

namespace odr::internal::font {

type1::Type2Charstring type1::to_type2(const std::string_view type1,
                                       const std::vector<std::string> &subrs) {
  return Translator(subrs).run(type1);
}

} // namespace odr::internal::font
