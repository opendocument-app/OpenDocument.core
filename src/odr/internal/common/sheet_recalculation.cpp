#include <odr/internal/common/sheet_recalculation.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document.hpp>
#include <odr/internal/common/sheet_cell_source.hpp>
#include <odr/internal/common/sheet_dependencies.hpp>
#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_function.hpp>
#include <odr/internal/formula/formula_parser.hpp>

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace odr::internal {

namespace {

/// A formula cell, and what it states.
struct Formula final {
  ElementIdentifier sheet_id{null_element_id};
  std::optional<formula::Node> node{};
  /// Whether a repeat or an array formula stands for the cell, which the
  /// evaluator cannot compute apart from the rest of the span.
  bool spanned{false};
  /// Whether the cell is one an array formula fills, not the formula's own.
  bool output{false};
};

/// The cells one formula stands for: a repeat, or an array formula's range.
struct Span final {
  SheetPosition first{};
  TableDimensions size{};
  bool array{false};
};

/// Calls @p visit with every position of @p span.
template <typename Visit>
void for_each_position(const Span &span, const Visit &visit) {
  for (std::uint32_t row = 0; row < span.size.rows; ++row) {
    for (std::uint32_t column = 0; column < span.size.columns; ++column) {
      visit(SheetPosition(span.first.sheet, span.first.cell.column + column,
                          span.first.cell.row + row));
    }
  }
}

/// The most positions of spanned formula cells a recalculation reads.
constexpr std::size_t span_limit = 1 << 20;

/// Maximum dependency depth per chunk; `compute` resolves deeper chains from
/// the far end.
constexpr std::size_t depth_limit = 64;

/// The cells as a recalculation reads them: a stale formula cell computed on
/// demand, every other one as the file caches it.
class StaleCells final : public formula::CellSource {
public:
  StaleCells(const SheetCellSource *base,
             const std::unordered_map<SheetPosition, Formula> *formulas,
             std::unordered_set<SheetPosition> stale)
      : m_base{base}, m_formulas{formulas}, m_stale{std::move(stale)},
        m_stale_ordered(m_stale.begin(), m_stale.end()) {
    std::ranges::sort(m_stale_ordered);
  }

  [[nodiscard]] std::optional<std::uint32_t>
  sheet(const std::string_view name) const override {
    return m_base->sheet(name);
  }
  /// A stale cell is computed where its position is read, so an area holding
  /// one is read position by position.
  void
  for_each_cell(const formula::Area &area,
                const std::function<void(const SheetPosition &,
                                         const std::optional<formula::Value> &)>
                    &visit) const override {
    if (holds_stale(area)) {
      CellSource::for_each_cell(area, visit);
    } else {
      m_base->for_each_cell(area, visit);
    }
  }
  [[nodiscard]] TableDimensions
  extent(const std::uint32_t sheet) const override {
    return m_base->extent(sheet);
  }
  [[nodiscard]] std::optional<formula::Node>
  name(const std::string_view name, const std::uint32_t sheet) const override {
    return m_base->name(name, sheet);
  }

  [[nodiscard]] std::optional<formula::Value>
  cell(const SheetPosition &position) const override {
    const auto formula = m_formulas->find(position);
    if (!m_stale.contains(position) || formula == m_formulas->end()) {
      return m_base->cell(position);
    }
    if (const auto known = m_results.find(position); known != m_results.end()) {
      return known->second;
    }
    if (const auto busy = std::ranges::find(m_ancestors, position);
        busy != m_ancestors.end()) {
      m_circular.insert(busy, m_ancestors.end());
      m_circular.insert(m_stack.begin(), m_stack.end());
      return std::nullopt;
    }
    if (const auto busy = std::ranges::find(m_stack, position);
        busy != m_stack.end()) {
      m_circular.insert(busy, m_stack.end());
      return std::nullopt;
    }
    if (!formula->second.node.has_value() || formula->second.spanned) {
      m_results.emplace(position, std::nullopt);
      return std::nullopt;
    }
    if (m_stack.size() >= depth_limit) {
      if (!m_frontier.has_value()) {
        m_frontier = position;
        m_frontier_path = m_stack;
      }
      return std::nullopt;
    }
    m_stack.push_back(position);
    const std::optional<formula::Value> result = formula::evaluate(
        *formula->second.node, position, *this, m_base->settings());
    m_stack.pop_back();
    // a result the depth limit cut short is computed again
    if (!m_frontier.has_value()) {
      m_results.emplace(position, result);
    }
    return result;
  }

  [[nodiscard]] bool is_circular(const SheetPosition &position) const {
    return m_circular.contains(position);
  }

  /// Computes deep chains in bounded chunks, retaining their ancestors for
  /// cycle detection across the depth limit.
  std::optional<formula::Value> compute(const SheetPosition &position) const {
    std::vector<std::pair<SheetPosition, std::size_t>> pending{{position, 0}};
    while (true) {
      const std::optional<formula::Value> result = cell(pending.back().first);
      if (const auto frontier = std::exchange(m_frontier, std::nullopt)) {
        const std::size_t previous = m_ancestors.size();
        m_ancestors.insert(m_ancestors.end(), m_frontier_path.begin(),
                           m_frontier_path.end());
        pending.emplace_back(*frontier, previous);
      } else {
        m_ancestors.resize(pending.back().second);
        pending.pop_back();
        if (pending.empty()) {
          return result;
        }
      }
    }
  }

private:
  [[nodiscard]] bool holds_stale(const formula::Area &area) const {
    const TablePosition &from = area.range.from();
    const TablePosition &to = area.range.to();
    for (auto it = std::ranges::lower_bound(
             m_stale_ordered, SheetPosition(area.sheet, 0, from.row));
         it != m_stale_ordered.end() && it->sheet == area.sheet &&
         it->cell.row <= to.row;
         ++it) {
      if (it->cell.column >= from.column && it->cell.column <= to.column) {
        return true;
      }
    }
    return false;
  }

  const SheetCellSource *m_base{nullptr};
  const std::unordered_map<SheetPosition, Formula> *m_formulas{nullptr};
  std::unordered_set<SheetPosition> m_stale;
  /// The stale positions in reading order.
  std::vector<SheetPosition> m_stale_ordered;
  mutable std::unordered_map<SheetPosition, std::optional<formula::Value>>
      m_results;
  mutable std::vector<SheetPosition> m_stack;
  mutable std::vector<SheetPosition> m_ancestors;
  mutable std::vector<SheetPosition> m_frontier_path;
  mutable std::unordered_set<SheetPosition> m_circular;
  mutable std::optional<SheetPosition> m_frontier;
};

/// @p value as a cell states it. A number is the serial the formula computes
/// with, which the writer shows by the cell's format.
CellValue cell_value_of(const formula::Value &value) {
  if (const auto *number = std::get_if<double>(&value.content)) {
    return CellValue(ValueType::float_number).with_number(*number);
  }
  if (const auto *text = std::get_if<std::string>(&value.content)) {
    return CellValue(*text);
  }
  if (const auto *boolean = std::get_if<bool>(&value.content)) {
    return CellValue(ValueType::boolean).with_number(*boolean ? 1 : 0);
  }
  if (const auto *error = std::get_if<formula::ErrorType>(&value.content)) {
    return CellValue(ValueType::error)
        .with_text(std::string(formula::to_string(*error)));
  }
  return CellValue(ValueType::float_number).with_number(0);
}

} // namespace

} // namespace odr::internal

namespace odr {

bool internal::is_edited(const abstract::Document &document) {
  const auto *edited = dynamic_cast<const Document *>(&document);
  return edited != nullptr && (edited->moved() || !edited->written().empty());
}

internal::SheetRecalculation
internal::recalculate(const abstract::Document &document) {
  SheetRecalculation result;
  const std::optional<formula::Syntax> syntax =
      formula::syntax_of(document.file_type());
  const abstract::ElementAdapter *adapter = document.element_adapter();
  if (!syntax.has_value() || adapter == nullptr) {
    return result;
  }
  const SheetCellSource source(document);

  std::unordered_map<ElementIdentifier, std::uint32_t> sheet_index;
  std::unordered_map<SheetPosition, Formula> formulas;
  // the formulas that stand for more than their own cell, or that the
  // evaluator does not compute (decision 29: an array formula)
  std::vector<Span> spans;
  std::size_t spanned_positions = 0;
  for (std::uint32_t index = 0; index < source.sheets().size(); ++index) {
    const ElementIdentifier sheet_id = source.sheets()[index].identifier();
    sheet_index.emplace(sheet_id, index);
    adapter->sheet_adapter(sheet_id)->sheet_visit_formulas(
        sheet_id, [&](const std::uint32_t column, const std::uint32_t row,
                      const TableDimensions &span, const bool array,
                      const std::string &text) {
          const SheetPosition position(index, column, row);
          const bool spanned = array || span.rows > 1 || span.columns > 1;
          formulas[position] =
              Formula{sheet_id, formula::parse(text, *syntax), spanned};
          if (spanned) {
            spans.push_back(Span{position, span, array});
            spanned_positions += std::size_t{span.rows} * span.columns;
          }
        });
  }
  if (spanned_positions > span_limit) {
    throw UnsupportedOperation();
  }
  for (const Span &span : spans) {
    const ElementIdentifier sheet_id = formulas.at(span.first).sheet_id;
    for_each_position(span, [&](const SheetPosition &position) {
      formulas[position] = Formula{sheet_id, std::nullopt, true,
                                   span.array && position != span.first};
    });
  }

  const SheetDependencies &graph = document.sheet_dependencies();
  const auto *edited = dynamic_cast<const Document *>(&document);
  std::vector<SheetPosition> stale;
  if (edited != nullptr && edited->moved()) {
    for (const auto &[position, formula] : formulas) {
      stale.push_back(position);
    }
  } else {
    if (edited != nullptr) {
      std::vector<SheetPosition> written;
      for (const auto &[sheet_id, position] : edited->written()) {
        if (const auto index = sheet_index.find(sheet_id);
            index != sheet_index.end()) {
          written.emplace_back(index->second, position);
        }
      }
      stale = graph.dependents(written);
    }
    for (const auto &[position, formula] : formulas) {
      if (!source.cell(position).has_value() ||
          (formula.node.has_value() && formula::is_volatile(*formula.node))) {
        stale.push_back(position);
      }
    }
    stale.insert(stale.end(), graph.unresolved().begin(),
                 graph.unresolved().end());
  }
  std::unordered_set<SheetPosition> stale_set(stale.begin(), stale.end());
  // what reads a stale cell is stale, and a span is stale where one position
  // of it is, until neither adds a cell
  for (std::size_t size = 0; size != stale_set.size();) {
    size = stale_set.size();
    const std::vector<SheetPosition> reached = graph.dependents(
        std::vector<SheetPosition>(stale_set.begin(), stale_set.end()));
    stale_set.insert(reached.begin(), reached.end());
    for (const Span &span : spans) {
      bool any = false;
      for_each_position(span, [&](const SheetPosition &position) {
        any = any || stale_set.contains(position);
      });
      if (any) {
        for_each_position(span, [&](const SheetPosition &position) {
          stale_set.insert(position);
        });
      }
    }
  }
  std::vector<SheetPosition> ordered;
  for (const SheetPosition &position : stale_set) {
    if (const auto formula = formulas.find(position);
        formula != formulas.end() && !formula->second.output) {
      ordered.push_back(position);
    }
  }
  std::ranges::sort(ordered);

  // every result is computed before any is written: the source reads the
  // results the file caches
  const StaleCells cells(&source, &formulas, std::move(stale_set));
  struct Computed final {
    SheetPosition position{};
    std::optional<formula::Value> before{};
    std::optional<formula::Value> value{};
  };
  std::vector<Computed> computed;
  computed.reserve(ordered.size());
  for (const SheetPosition &position : ordered) {
    computed.push_back(
        Computed{position, source.cell(position), cells.compute(position)});
  }

  for (const auto &[position, before, value] : computed) {
    const ElementIdentifier sheet_id = formulas.at(position).sheet_id;
    const abstract::SheetAdapter *sheet = adapter->sheet_adapter(sheet_id);
    if (!value.has_value()) {
      (cells.is_circular(position) ? result.circular : result.unevaluated)
          .push_back(position);
      // the result the file caches is stale, where the file can drop it
      try {
        sheet->sheet_set_result(sheet_id, position.cell.column,
                                position.cell.row,
                                CellValue(ValueType::unknown));
      } catch (const UnsupportedOperation &) {
      }
      continue;
    }
    try {
      sheet->sheet_set_result(sheet_id, position.cell.column, position.cell.row,
                              cell_value_of(*value));
    } catch (const UnsupportedOperation &) {
      result.unevaluated.push_back(position);
      continue;
    }
    if (before != value) {
      result.changed.push_back(position);
    }
  }

  if (edited != nullptr) {
    edited->forget_edits();
  }
  return result;
}

} // namespace odr
