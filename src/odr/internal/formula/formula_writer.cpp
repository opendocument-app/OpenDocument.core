#include <odr/internal/formula/formula_writer.hpp>

#include <odr/table_position.hpp>

#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include <fmt/format.h>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

/// How tightly an operator binds, lowest first. A child binding less tightly
/// than its parent needs the parenthesis the tree no longer carries.
enum Precedence : std::uint8_t {
  comparison,
  concatenation,
  additive,
  multiplicative,
  exponentiation,
  sign,
  percent,
  reference,
  primary,
};

Precedence precedence_of(const BinaryOperator op) {
  switch (op) {
  case BinaryOperator::equal:
  case BinaryOperator::not_equal:
  case BinaryOperator::less:
  case BinaryOperator::less_equal:
  case BinaryOperator::greater:
  case BinaryOperator::greater_equal:
    return Precedence::comparison;
  case BinaryOperator::concat:
    return Precedence::concatenation;
  case BinaryOperator::add:
  case BinaryOperator::subtract:
    return Precedence::additive;
  case BinaryOperator::multiply:
  case BinaryOperator::divide:
    return Precedence::multiplicative;
  case BinaryOperator::power:
    return Precedence::exponentiation;
  case BinaryOperator::range:
  case BinaryOperator::intersect:
  case BinaryOperator::unite:
    return Precedence::reference;
  }
  return Precedence::primary;
}

Precedence precedence_of(const Node &node) {
  if (node.holds<BinaryOperation>()) {
    return precedence_of(node.get<BinaryOperation>().op);
  }
  if (node.holds<UnaryOperation>()) {
    return node.get<UnaryOperation>().op == UnaryOperator::percent
               ? Precedence::percent
               : Precedence::sign;
  }
  return Precedence::primary;
}

std::string_view spelling_of(const BinaryOperator op) {
  switch (op) {
  case BinaryOperator::add:
    return "+";
  case BinaryOperator::subtract:
    return "-";
  case BinaryOperator::multiply:
    return "*";
  case BinaryOperator::divide:
    return "/";
  case BinaryOperator::power:
    return "^";
  case BinaryOperator::concat:
    return "&";
  case BinaryOperator::equal:
    return "=";
  case BinaryOperator::not_equal:
    return "<>";
  case BinaryOperator::less:
    return "<";
  case BinaryOperator::less_equal:
    return "<=";
  case BinaryOperator::greater:
    return ">";
  case BinaryOperator::greater_equal:
    return ">=";
  case BinaryOperator::range:
    return ":";
  case BinaryOperator::intersect:
    return "!";
  case BinaryOperator::unite:
    return "~";
  }
  return "+";
}

std::string_view spelling_of(const ErrorType type) {
  switch (type) {
  case ErrorType::null:
    return "#NULL!";
  case ErrorType::division:
    return "#DIV/0!";
  case ErrorType::value:
    return "#VALUE!";
  case ErrorType::reference:
    return "#REF!";
  case ErrorType::name:
    return "#NAME?";
  case ErrorType::number:
    return "#NUM!";
  case ErrorType::not_available:
    return "#N/A";
  }
  return "#NULL!";
}

/// A quote inside a quoted run is written twice, which is how both syntaxes
/// escape one.
std::string quote(const std::string_view text, const char mark) {
  std::string result(1, mark);
  for (const char c : text) {
    if (c == mark) {
      result += mark;
    }
    result += c;
  }
  result += mark;
  return result;
}

/// A sheet name keeps its quotes only where it needs them: one holding
/// anything but a letter, a digit or `_`, or opening with a digit, is quoted.
bool needs_quotes(const std::string_view name) {
  return name.empty() || str::is_ascii_digit(name.front()) ||
         !std::ranges::all_of(name, [](const char c) {
           return str::is_ascii_letter_or_digit(c) || c == '_';
         });
}

std::string spell_coordinate(const std::optional<Coordinate> &coordinate,
                             const bool column) {
  if (!coordinate.has_value()) {
    return "";
  }
  return (coordinate->absolute ? "$" : "") +
         (column ? TablePosition::to_column_string(coordinate->index)
                 : TablePosition::to_row_string(coordinate->index));
}

class Writer final {
public:
  explicit Writer(const Syntax syntax) : m_syntax{syntax} {}

  [[nodiscard]] std::string write(const Node &node) const {
    return std::visit(
        [&](const auto &content) { return write_content(content, node); },
        node.content);
  }

private:
  Syntax m_syntax{Syntax::ooxml};

  [[nodiscard]] char separator() const {
    return m_syntax == Syntax::opendocument ? ';' : ',';
  }

  /// The child, parenthesised where its operator binds less tightly than the
  /// one above — or equally, on the right: `1-(2-3)` is not `1-2-3`.
  [[nodiscard]] std::string nested(const Node &child, const Precedence parent,
                                   const bool right) const {
    const Precedence own = precedence_of(child);
    if (own < parent || (right && own == parent)) {
      return "(" + write(child) + ")";
    }
    return write(child);
  }

  [[nodiscard]] std::string write_content(const NumberLiteral &content,
                                          const Node &) const {
    return fmt::format("{}", content.value);
  }

  [[nodiscard]] std::string write_content(const StringLiteral &content,
                                          const Node &) const {
    return quote(content.value, '"');
  }

  [[nodiscard]] std::string write_content(const BooleanLiteral &content,
                                          const Node &) const {
    if (m_syntax == Syntax::opendocument) {
      return content.value ? "TRUE()" : "FALSE()";
    }
    return content.value ? "TRUE" : "FALSE";
  }

  [[nodiscard]] std::string write_content(const ErrorLiteral &content,
                                          const Node &) const {
    return std::string(spelling_of(content.type));
  }

  [[nodiscard]] std::string write_content(const Missing &, const Node &) const {
    return "";
  }

  [[nodiscard]] std::string write_content(const CellReference &content,
                                          const Node &) const {
    if (m_syntax == Syntax::opendocument) {
      return "[" + locator(content, true) + "]";
    }
    return locator(content, true);
  }

  [[nodiscard]] std::string write_content(const RangeReference &content,
                                          const Node &) const {
    if (m_syntax == Syntax::opendocument) {
      return "[" + locator(content.from, true) + ":" +
             locator(content.to, true) + "]";
    }
    return locator(content.from, true) + ":" + locator(content.to, false);
  }

  [[nodiscard]] std::string write_content(const NameReference &content,
                                          const Node &) const {
    if (m_syntax == Syntax::opendocument) {
      return "$$" + (needs_quotes(content.name) ? quote(content.name, '\'')
                                                : content.name);
    }
    return qualifier(content.document, content.sheet, false) + content.name;
  }

  [[nodiscard]] std::string write_content(const FunctionCall &content,
                                          const Node &node) const {
    std::string result = content.name + "(";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
      if (i > 0) {
        result += separator();
      }
      result += write(node.children[i]);
    }
    return result + ")";
  }

  [[nodiscard]] std::string write_content(const UnaryOperation &content,
                                          const Node &node) const {
    const Node &operand = node.children.front();
    if (content.op == UnaryOperator::percent) {
      return nested(operand, Precedence::percent, false) + "%";
    }
    return (content.op == UnaryOperator::minus ? "-" : "+") +
           nested(operand, Precedence::sign, false);
  }

  [[nodiscard]] std::string write_content(const BinaryOperation &content,
                                          const Node &node) const {
    const Precedence own = precedence_of(content.op);
    // ooxml spells a union with the comma it also separates arguments with,
    // so the parenthesis is what tells the two apart
    if (m_syntax == Syntax::ooxml && content.op == BinaryOperator::unite) {
      return "(" + write(node.children.front()) + "," +
             write(node.children.back()) + ")";
    }
    const std::string_view spelling =
        m_syntax == Syntax::ooxml && content.op == BinaryOperator::intersect
            ? " "
            : spelling_of(content.op);
    return nested(node.children.front(), own, false) + std::string(spelling) +
           nested(node.children.back(), own, true);
  }

  [[nodiscard]] std::string write_content(const ArrayLiteral &content,
                                          const Node &node) const {
    const char row_separator = m_syntax == Syntax::opendocument ? '|' : ';';
    std::string result = "{";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
      if (i > 0) {
        result += content.columns != 0 && i % content.columns == 0
                      ? row_separator
                      : separator();
      }
      result += write(node.children[i]);
    }
    return result + "}";
  }

  /// The document and the sheet in front of a reference or a name.
  [[nodiscard]] std::string
  qualifier(const std::optional<std::string> &document,
            const std::optional<std::string> &sheet,
            const bool sheet_absolute) const {
    if (m_syntax == Syntax::opendocument) {
      std::string result;
      if (document.has_value()) {
        result += quote(*document, '\'') + "#";
      }
      if (sheet_absolute) {
        result += "$";
      }
      if (sheet.has_value()) {
        result += needs_quotes(*sheet) ? quote(*sheet, '\'') : *sheet;
      }
      return result;
    }
    std::string result;
    if (document.has_value()) {
      result += "[" + *document + "]";
    }
    if (sheet.has_value()) {
      result += (needs_quotes(*sheet) ? quote(*sheet, '\'') : *sheet) + "!";
    } else if (document.has_value()) {
      // `[1]!Total` names the other workbook itself, with no sheet between
      result += "!";
    }
    return result;
  }

  /// One corner of a reference. The second corner of an ooxml range drops a
  /// qualifier the first already states.
  [[nodiscard]] std::string locator(const CellReference &cell,
                                    const bool qualified) const {
    std::string coordinates =
        spell_coordinate(cell.column, true) + spell_coordinate(cell.row, false);
    if (m_syntax == Syntax::opendocument) {
      return qualifier(cell.document, cell.sheet, cell.sheet_absolute) + "." +
             coordinates;
    }
    if (!qualified) {
      return coordinates;
    }
    return qualifier(cell.document, cell.sheet, cell.sheet_absolute) +
           coordinates;
  }
};

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

std::string formula::to_string(const formula::Node &node,
                               const formula::Syntax syntax) {
  return formula::Writer(syntax).write(node);
}

} // namespace odr::internal
