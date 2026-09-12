#include <odr/internal/formula/formula_parser.hpp>

#include <odr/table_position.hpp>

#include <odr/internal/common/text_cursor.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <utility>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

/// What a function name, a sheet name, a named expression and a cell
/// reference are all spelled out of. `_xlfn.FLOOR.MATH` is one run of these.
bool is_name_char(const char c) {
  return str::is_ascii_letter_or_digit(c) || c == '_' || c == '.' || c == '$' ||
         c == '\\';
}

/// `$A$1`, `A1`, and the half a whole column or row states: `A`, `1`. Takes
/// from @p text what it reads and leaves the rest.
std::optional<CellReference> take_coordinates(std::string_view &text,
                                              CellReference reference) {
  const auto character = [&text](const std::size_t i) {
    return i < text.size() ? text[i] : '\0';
  };
  const bool column_absolute = character(0) == '$';
  const std::size_t first = column_absolute ? 1 : 0;
  std::size_t letters = first;
  while (str::is_ascii_letter(character(letters))) {
    ++letters;
  }
  const bool row_absolute =
      letters > first ? character(letters) == '$' : column_absolute;
  std::size_t after = letters + (letters > first && row_absolute ? 1 : 0);
  std::size_t digits = after;
  while (str::is_ascii_digit(character(digits))) {
    ++digits;
  }

  if (letters > first) {
    const std::optional<std::uint32_t> column =
        TablePosition::try_to_column_num(text.substr(first, letters - first));
    if (!column.has_value()) {
      return {};
    }
    reference.column = Coordinate{*column, column_absolute};
  }
  if (digits > after) {
    const std::optional<std::uint32_t> row =
        TablePosition::try_to_row_num(text.substr(after, digits - after));
    if (!row.has_value()) {
      return {};
    }
    reference.row = Coordinate{*row, row_absolute};
  } else if (letters > first) {
    // a column on its own: `$A` states nothing about a row
    digits = letters;
  }
  if (!reference.column.has_value() && !reference.row.has_value()) {
    return {};
  }
  text.remove_prefix(digits);
  return reference;
}

struct ErrorSpelling final {
  std::string_view text;
  ErrorType type;
};

constexpr std::array<ErrorSpelling, 7> error_spellings{{
    {"#DIV/0!", ErrorType::division},
    {"#VALUE!", ErrorType::value},
    {"#NAME?", ErrorType::name},
    {"#NULL!", ErrorType::null},
    {"#NUM!", ErrorType::number},
    {"#REF!", ErrorType::reference},
    {"#N/A", ErrorType::not_available},
}};

Node make(Node::Content content, std::vector<Node> children = {}) {
  return Node{std::move(content), std::move(children)};
}

Node make_unary(const UnaryOperator op, Node operand) {
  std::vector<Node> children;
  children.push_back(std::move(operand));
  return make(UnaryOperation{op}, std::move(children));
}

std::optional<Node> make_binary(const BinaryOperator op, Node left,
                                std::optional<Node> right) {
  if (!right.has_value()) {
    return {};
  }
  std::vector<Node> children;
  children.push_back(std::move(left));
  children.push_back(std::move(*right));
  return make(BinaryOperation{op}, std::move(children));
}

/// Recursive descent over the grammar the two syntaxes share. A production
/// that does not parse answers nothing, and the whole parse fails with it.
class Parser final : private TextCursor {
public:
  Parser(const std::string_view input, const Syntax syntax)
      : TextCursor{input}, m_syntax{syntax} {}

  [[nodiscard]] std::optional<Node> parse() {
    std::optional<Node> node = expression();
    skip_whitespace();
    if (!node.has_value() || !empty()) {
      return {};
    }
    return node;
  }

private:
  Syntax m_syntax{Syntax::ooxml};

  [[nodiscard]] std::string_view take_name() {
    return take_while(is_name_char);
  }

  /// The argument separator of the dialect: OpenFormula writes `;`, ooxml `,`.
  [[nodiscard]] char separator() const {
    return m_syntax == Syntax::opendocument ? ';' : ',';
  }

  [[nodiscard]] std::optional<Node> expression() { return comparison(); }

  [[nodiscard]] std::optional<Node> comparison() {
    std::optional<Node> left = concatenation();
    while (left.has_value()) {
      BinaryOperator op{};
      if (consume("<>")) {
        op = BinaryOperator::not_equal;
      } else if (consume("<=")) {
        op = BinaryOperator::less_equal;
      } else if (consume(">=")) {
        op = BinaryOperator::greater_equal;
      } else if (consume('=')) {
        op = BinaryOperator::equal;
      } else if (consume('<')) {
        op = BinaryOperator::less;
      } else if (consume('>')) {
        op = BinaryOperator::greater;
      } else {
        break;
      }
      left = make_binary(op, std::move(*left), concatenation());
    }
    return left;
  }

  [[nodiscard]] std::optional<Node> concatenation() {
    std::optional<Node> left = additive();
    while (left.has_value() && consume('&')) {
      left = make_binary(BinaryOperator::concat, std::move(*left), additive());
    }
    return left;
  }

  [[nodiscard]] std::optional<Node> additive() {
    std::optional<Node> left = multiplicative();
    while (left.has_value()) {
      skip_whitespace();
      BinaryOperator op{};
      if (peek() == '+') {
        op = BinaryOperator::add;
      } else if (peek() == '-') {
        op = BinaryOperator::subtract;
      } else {
        break;
      }
      advance(1);
      left = make_binary(op, std::move(*left), multiplicative());
    }
    return left;
  }

  [[nodiscard]] std::optional<Node> multiplicative() {
    std::optional<Node> left = power();
    while (left.has_value()) {
      skip_whitespace();
      BinaryOperator op{};
      if (peek() == '*') {
        op = BinaryOperator::multiply;
      } else if (peek() == '/') {
        op = BinaryOperator::divide;
      } else {
        break;
      }
      advance(1);
      left = make_binary(op, std::move(*left), power());
    }
    return left;
  }

  /// `-2^2` is 4: a sign binds tighter than the power, as it does in a sheet.
  [[nodiscard]] std::optional<Node> power() {
    std::optional<Node> left = unary();
    while (left.has_value() && consume('^')) {
      left = make_binary(BinaryOperator::power, std::move(*left), unary());
    }
    return left;
  }

  [[nodiscard]] std::optional<Node> unary() {
    skip_whitespace();
    if (peek() == '-' || peek() == '+') {
      const UnaryOperator op =
          peek() == '-' ? UnaryOperator::minus : UnaryOperator::plus;
      advance(1);
      std::optional<Node> operand = unary();
      if (!operand.has_value()) {
        return {};
      }
      return make_unary(op, std::move(*operand));
    }
    return postfix();
  }

  [[nodiscard]] std::optional<Node> postfix() {
    std::optional<Node> node = reference_expression();
    while (node.has_value() && consume('%')) {
      node = make_unary(UnaryOperator::percent, std::move(*node));
    }
    return node;
  }

  /// The reference operators, tighter than everything above them. A range is
  /// usually one token; this joins the two halves a formula spells apart.
  [[nodiscard]] std::optional<Node> reference_expression() {
    std::optional<Node> left = primary();
    while (left.has_value()) {
      skip_whitespace();
      BinaryOperator op{};
      if (peek() == ':') {
        op = BinaryOperator::range;
      } else if (m_syntax == Syntax::opendocument && peek() == '!') {
        op = BinaryOperator::intersect;
      } else if (m_syntax == Syntax::opendocument && peek() == '~') {
        op = BinaryOperator::unite;
      } else {
        break;
      }
      advance(1);
      left = make_binary(op, std::move(*left), primary());
    }
    return left;
  }

  [[nodiscard]] std::optional<Node> primary() {
    skip_whitespace();
    const char c = peek();
    if (c == '(') {
      return group();
    }
    if (c == '{') {
      return array();
    }
    if (c == '"') {
      return string_literal();
    }
    if (c == '#') {
      return error_literal();
    }
    if (str::is_ascii_digit(c) || (c == '.' && str::is_ascii_digit(peek(1)))) {
      return number_literal();
    }
    if (m_syntax == Syntax::opendocument) {
      return opendocument_primary();
    }
    return ooxml_primary();
  }

  /// A parenthesised expression, and in ooxml the union a comma inside it
  /// writes: `SUM((A1:A2,B1:B2))`.
  [[nodiscard]] std::optional<Node> group() {
    if (!consume('(')) {
      return {};
    }
    std::optional<Node> node = expression();
    while (node.has_value() && m_syntax == Syntax::ooxml && consume(',')) {
      node = make_binary(BinaryOperator::unite, std::move(*node), expression());
    }
    if (!node.has_value() || !consume(')')) {
      return {};
    }
    return node;
  }

  [[nodiscard]] std::optional<Node> array() {
    if (!consume('{')) {
      return {};
    }
    const char row_separator = m_syntax == Syntax::opendocument ? '|' : ';';
    std::vector<Node> elements;
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    std::uint32_t in_row = 0;
    while (true) {
      std::optional<Node> element = expression();
      if (!element.has_value()) {
        return {};
      }
      elements.push_back(std::move(*element));
      ++in_row;
      if (consume(separator())) {
        continue;
      }
      if (columns != 0 && columns != in_row) {
        return {};
      }
      columns = in_row;
      in_row = 0;
      ++rows;
      if (consume(row_separator)) {
        continue;
      }
      break;
    }
    if (!consume('}')) {
      return {};
    }
    return make(ArrayLiteral{columns, rows}, std::move(elements));
  }

  [[nodiscard]] std::optional<Node> string_literal() {
    std::optional<std::string> text = quoted('"');
    if (!text.has_value()) {
      return {};
    }
    return make(StringLiteral{std::move(*text)});
  }

  /// A quoted run, a doubled quote standing for the quote itself. The cursor
  /// does not move where the quote never closes.
  [[nodiscard]] std::optional<std::string> quoted(const char quote) {
    if (peek() != quote) {
      return {};
    }
    const std::string_view run = rest();
    std::string text;
    std::size_t at = 1;
    while (at < run.size()) {
      const char c = run[at];
      if (c != quote) {
        text += c;
        ++at;
        continue;
      }
      if (at + 1 < run.size() && run[at + 1] == quote) {
        text += quote;
        at += 2;
        continue;
      }
      advance(at + 1);
      return text;
    }
    return {};
  }

  [[nodiscard]] std::optional<Node> error_literal() {
    for (const ErrorSpelling &spelling : error_spellings) {
      if (rest().starts_with(spelling.text)) {
        advance(spelling.text.size());
        return make(ErrorLiteral{spelling.type});
      }
    }
    return {};
  }

  /// Digits, one `.`, an exponent. The decimal separator is the `.` both
  /// syntaxes state, whatever the document's locale shows.
  [[nodiscard]] std::optional<Node> number_literal() {
    std::size_t length = 0;
    while (str::is_ascii_digit(peek(length))) {
      ++length;
    }
    if (peek(length) == '.') {
      ++length;
      while (str::is_ascii_digit(peek(length))) {
        ++length;
      }
    }
    if (peek(length) == 'e' || peek(length) == 'E') {
      std::size_t exponent = length + 1;
      if (peek(exponent) == '+' || peek(exponent) == '-') {
        ++exponent;
      }
      if (str::is_ascii_digit(peek(exponent))) {
        while (str::is_ascii_digit(peek(exponent))) {
          ++exponent;
        }
        length = exponent;
      }
    }
    // `strtod` wants a terminator, which the view does not promise
    const std::string text(rest().substr(0, length));
    char *end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size()) {
      return {};
    }
    advance(length);
    return make(NumberLiteral{value});
  }

  /// `[.A1]`, `[Sheet1.A1:.B2]`, `[#REF!]`, `$$Name`, `SUM(`, `TRUE`.
  [[nodiscard]] std::optional<Node> opendocument_primary() {
    if (peek() == '[') {
      return opendocument_reference();
    }
    if (peek() == '$' && peek(1) == '$') {
      advance(2);
      if (const std::optional<std::string> quoted_name = quoted('\'');
          quoted_name.has_value()) {
        return make(NameReference{.name = *quoted_name});
      }
      const std::string_view name = take_name();
      if (name.empty()) {
        return {};
      }
      return make(NameReference{.name = std::string(name)});
    }
    const std::string_view name = take_name();
    if (name.empty()) {
      return {};
    }
    if (peek() == '(') {
      return function_call(std::string(name));
    }
    const bool boolean_true = str::equals_ignore_case(name, "TRUE");
    if (boolean_true || str::equals_ignore_case(name, "FALSE")) {
      return make(BooleanLiteral{boolean_true});
    }
    return make(NameReference{.name = std::string(name)});
  }

  /// What `[` encloses: one locator, or two around a `:`, or the error a
  /// deleted target left behind.
  [[nodiscard]] std::optional<Node> opendocument_reference() {
    if (!consume('[')) {
      return {};
    }
    skip_whitespace();
    if (peek() == '#') {
      std::optional<Node> error = error_literal();
      if (!error.has_value() || !consume(']')) {
        return {};
      }
      return error;
    }
    const std::optional<CellReference> from = opendocument_locator();
    if (!from.has_value()) {
      return {};
    }
    if (consume(':')) {
      const std::optional<CellReference> to = opendocument_locator();
      if (!to.has_value() || !consume(']')) {
        return {};
      }
      return make(RangeReference{*from, *to});
    }
    if (!consume(']')) {
      return {};
    }
    if (!from->column.has_value() || !from->row.has_value()) {
      return {};
    }
    return make(*from);
  }

  /// `['file:///x.ods'#$Sheet1.A1]`: the document, the sheet and the cell,
  /// every part of it optional but the cell's own `.`.
  [[nodiscard]] std::optional<CellReference> opendocument_locator() {
    CellReference reference;
    skip_whitespace();
    if (peek() == '\'') {
      const std::optional<std::string> text = quoted('\'');
      if (!text.has_value()) {
        return {};
      }
      if (consume('#')) {
        reference.document = *text;
      } else {
        reference.sheet = *text;
      }
    }
    if (!reference.sheet.has_value()) {
      if (peek() == '$') {
        reference.sheet_absolute = true;
        advance(1);
      }
      if (peek() == '\'') {
        const std::optional<std::string> text = quoted('\'');
        if (!text.has_value()) {
          return {};
        }
        reference.sheet = *text;
      } else if (const std::string_view name = take_sheet_name();
                 !name.empty()) {
        reference.sheet = std::string(name);
      }
    }
    if (!consume('.')) {
      return {};
    }
    std::string_view text = rest();
    const std::optional<CellReference> read =
        take_coordinates(text, std::move(reference));
    seek(text);
    return read;
  }

  /// An unquoted sheet name, which ends at the `.` in front of the cell.
  [[nodiscard]] std::string_view take_sheet_name() {
    return take_while(
        [](const char c) { return c != '.' && c != ':' && c != ']'; });
  }

  /// `[1]Sheet1!A1:B2`, `'My Sheet'!A1`, `SUM(`, `TRUE`, a named expression.
  [[nodiscard]] std::optional<Node> ooxml_primary() {
    std::optional<std::string> document;
    if (peek() == '[') {
      const std::size_t close = rest().find(']');
      if (close == std::string_view::npos) {
        return {};
      }
      document = std::string(rest().substr(1, close - 1));
      advance(close + 1);
      // `[1]!Name` names the other workbook itself, with no sheet between
      if (peek() == '!') {
        advance(1);
      }
    }
    const std::optional<std::string> sheet = ooxml_sheet();
    if (peek() == '#') {
      return error_literal();
    }

    const std::string_view name = take_name();
    if (name.empty()) {
      return {};
    }
    if (peek() == '(' && !sheet.has_value()) {
      return function_call(std::string(name));
    }

    std::string_view rest = name;
    const std::optional<CellReference> from = take_coordinates(
        rest, CellReference{.document = document, .sheet = sheet});
    const bool whole = from.has_value() && rest.empty();
    const bool complete =
        whole && from->column.has_value() && from->row.has_value();

    if (whole && peek() == ':') {
      advance(1);
      std::string_view tail = take_name();
      const std::optional<CellReference> to =
          take_coordinates(tail, CellReference{});
      if (!to.has_value() || !tail.empty() ||
          from->column.has_value() != to->column.has_value() ||
          from->row.has_value() != to->row.has_value()) {
        return {};
      }
      return make(RangeReference{*from, *to});
    }
    if (complete) {
      return make(*from);
    }
    const bool boolean_true = str::equals_ignore_case(name, "TRUE");
    if (!sheet.has_value() && !document.has_value() &&
        (boolean_true || str::equals_ignore_case(name, "FALSE"))) {
      return make(BooleanLiteral{boolean_true});
    }
    return make(NameReference{
        .document = document, .sheet = sheet, .name = std::string(name)});
  }

  /// The `Sheet1!` a reference may carry. A span over several sheets
  /// (`'A B':'C D'!`) stays the spelling the file states, quotes and all.
  [[nodiscard]] std::optional<std::string> ooxml_sheet() {
    const std::string_view start = rest();
    const std::optional<std::string> first = quoted('\'');
    std::string spelled;
    if (first.has_value()) {
      spelled = "'" + *first + "'";
    } else if (const std::string_view name = take_name(); !name.empty()) {
      spelled = std::string(name);
    } else {
      seek(start);
      return {};
    }
    if (peek() == '!') {
      advance(1);
      return first.has_value() ? *first : spelled;
    }
    if (peek() == ':') {
      advance(1);
      std::string span = spelled + ":";
      if (const std::optional<std::string> second = quoted('\'');
          second.has_value()) {
        span += "'" + *second + "'";
      } else if (const std::string_view name = take_name(); !name.empty()) {
        span += std::string(name);
      } else {
        seek(start);
        return {};
      }
      if (peek() == '!') {
        advance(1);
        return span;
      }
    }
    seek(start);
    return {};
  }

  [[nodiscard]] std::optional<Node> function_call(std::string name) {
    if (!consume('(')) {
      return {};
    }
    std::vector<Node> arguments;
    if (consume(')')) {
      return make(FunctionCall{std::move(name)}, std::move(arguments));
    }
    while (true) {
      skip_whitespace();
      if (peek() == separator() || peek() == ')') {
        arguments.push_back(make(Missing{}));
      } else {
        std::optional<Node> argument = expression();
        if (!argument.has_value()) {
          return {};
        }
        arguments.push_back(std::move(*argument));
      }
      if (consume(separator())) {
        continue;
      }
      if (consume(')')) {
        break;
      }
      return {};
    }
    return make(FunctionCall{std::move(name)}, std::move(arguments));
  }
};

/// The `of:` a `table:formula` carries, and the `=` both may. The prefix is
/// the producer's namespace, so it is whatever name it declared.
std::string_view strip_prefix(std::string_view formula) {
  if (const std::size_t assign = formula.find(":=");
      assign != std::string_view::npos &&
      std::ranges::all_of(formula.substr(0, assign), [](const char c) {
        return str::is_ascii_letter(c) || str::is_ascii_digit(c) || c == '_' ||
               c == '-';
      })) {
    return formula.substr(assign + 2);
  }
  if (formula.starts_with('=')) {
    return formula.substr(1);
  }
  return formula;
}

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

std::optional<formula::Node> formula::parse(const std::string_view formula,
                                            const formula::Syntax syntax) {
  return formula::Parser(formula::strip_prefix(formula), syntax).parse();
}

} // namespace odr::internal
