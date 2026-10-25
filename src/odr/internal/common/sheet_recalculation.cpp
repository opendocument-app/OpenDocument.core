#include <odr/internal/common/sheet_recalculation.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/common/document.hpp>
#include <odr/internal/common/sheet_cell_source.hpp>
#include <odr/internal/common/sheet_dependencies.hpp>
#include <odr/internal/formula/formula_dependencies.hpp>
#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_function.hpp>
#include <odr/internal/formula/formula_parser.hpp>

#include <odr/internal/number_format/number_format.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace odr::internal {

namespace {

/// The cells one formula stands for: a repeat, or an array formula's range.
struct Span final {
  SheetPosition first{};
  TableDimensions size{};
  bool array{false};
};

/// A formula cell, and what it states.
struct Formula final {
  ElementIdentifier sheet_id{null_element_id};
  std::optional<formula::Node> node{};
  /// Whether a repeat stands for the cell, which the evaluator cannot
  /// compute apart from the rest of the span.
  bool spanned{false};
  /// Whether the cell is one an array formula fills, not the formula's own.
  bool output{false};
  /// The range of the array formula that fills the cell.
  std::optional<Span> array{};
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

/// Days since 1899-12-30 of this moment in the local time of the machine, as
/// a spreadsheet application computes `NOW()`.
double local_now() {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  return static_cast<double>(number_format::days_from_civil(
             local.tm_year + 1900, static_cast<std::uint32_t>(local.tm_mon + 1),
             static_cast<std::uint32_t>(local.tm_mday))) +
         (local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec) / 86400.0;
}

/// The settings of the document, with the clock and the random numbers a
/// recalculation computes `NOW` and `RAND` with.
formula::Settings recalculating(formula::Settings settings) {
  settings.now = local_now();
  settings.random =
      [generator = std::make_shared<std::mt19937_64>(std::random_device{}())] {
        return std::uniform_real_distribution<double>(0, 1)(*generator);
      };
  return settings;
}

/// The cells as a recalculation reads them: a stale formula cell computed on
/// demand, every other one as the file caches it.
class StaleCells final : public formula::CellSource {
public:
  StaleCells(const SheetCellSource *base,
             const std::unordered_map<SheetPosition, Formula> *formulas,
             std::unordered_set<SheetPosition> stale)
      : m_base{base}, m_settings{recalculating(base->settings())},
        m_formulas{formulas}, m_stale{std::move(stale)},
        m_stale_ordered(m_stale.begin(), m_stale.end()) {
    std::ranges::sort(m_stale_ordered);
  }

  [[nodiscard]] std::optional<std::uint32_t>
  sheet(const std::string_view name) const override {
    return m_base->sheet(name);
  }
  /// A stale cell is computed where its position is read, so an area holding
  /// one is read position by position.
  [[nodiscard]] formula::Result<void> for_each_cell(
      const formula::Area &area,
      const std::function<formula::Result<void>(
          const SheetPosition &, const std::optional<formula::Value> &)> &visit)
      const override {
    if (holds_stale(area)) {
      return CellSource::for_each_cell(area, visit);
    }
    return m_base->for_each_cell(area, visit);
  }
  [[nodiscard]] TableDimensions
  extent(const std::uint32_t sheet) const override {
    return m_base->extent(sheet);
  }
  [[nodiscard]] std::optional<formula::Node>
  name(const std::string_view name, const std::uint32_t sheet) const override {
    return m_base->name(name, sheet);
  }
  [[nodiscard]] std::optional<std::uint32_t>
  external_sheet(const std::string_view document,
                 const std::string_view sheet) const override {
    return m_base->external_sheet(document, sheet);
  }

  [[nodiscard]] std::optional<formula::Value>
  cell(const SheetPosition &position) const override {
    const auto formula = m_formulas->find(position);
    if (!m_stale.contains(position) || formula == m_formulas->end()) {
      return m_base->cell(position);
    }
    if (const auto iterated = m_iterated.find(position);
        iterated != m_iterated.end()) {
      return iterated->second;
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
    const Formula &entry = formula->second;
    if (entry.output && entry.array.has_value()) {
      // the formula's own cell computes the whole range
      static_cast<void>(cell(entry.array->first));
      const auto known = m_results.find(position);
      return known != m_results.end() ? known->second : std::nullopt;
    }
    if (!entry.node.has_value() || entry.spanned) {
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
    const std::optional<formula::Value> result =
        entry.array.has_value()
            ? fill(*entry.array, formula::evaluate_array(*entry.node, position,
                                                         *this, m_settings))
            : formula::evaluate(*entry.node, position, *this, m_settings);
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
      if (const auto frontier = std::exchange(m_frontier, std::nullopt);
          frontier.has_value()) {
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

  /// Computes the cycles the first pass met as the document's settings state:
  /// each of their cells in reading order, from what it holds, until none
  /// changes by the difference. Each result is computed again after it. False
  /// where the document computes no cycle, or meets none.
  bool iterate() const {
    if (!m_settings.iteration.has_value() || m_circular.empty()) {
      return false;
    }
    std::vector<SheetPosition> cells(m_circular.begin(), m_circular.end());
    std::ranges::sort(cells);
    for (const SheetPosition &position : cells) {
      const std::optional<formula::Value> held = m_base->cell(position);
      m_iterated[position] = held.has_value() && !held->holds<formula::Empty>()
                                 ? *held
                                 : formula::Value{0.0};
    }
    for (const std::vector<SheetPosition> &cycle : cycles(cells)) {
      iterate(cycle, *m_settings.iteration);
    }
    forget();
    return true;
  }

private:
  /// The cells of @p cells that read each other, each group after the groups
  /// it reads (Kosaraju). A cell that may read any cell reads all of them.
  [[nodiscard]] std::vector<std::vector<SheetPosition>>
  cycles(const std::vector<SheetPosition> &cells) const {
    const std::size_t count = cells.size();
    std::vector<std::vector<std::size_t>> reads(count);
    std::vector<std::vector<std::size_t>> readers(count);
    for (std::size_t i = 0; i < count; ++i) {
      const std::optional<formula::Node> &node = m_formulas->at(cells[i]).node;
      const formula::References read = node.has_value()
                                           ? formula::references(*node)
                                           : formula::References{{}, false};
      for (std::size_t j = 0; j < count; ++j) {
        const bool reached =
            !read.complete ||
            std::ranges::any_of(read.extents, [&](const formula::Extent &x) {
              if (x.document.has_value()) {
                return false;
              }
              const std::optional<std::uint32_t> sheet =
                  x.sheet.has_value() ? m_base->sheet(*x.sheet)
                                      : std::optional(cells[i].sheet);
              return sheet == cells[j].sheet && x.range.contains(cells[j].cell);
            });
        if (reached) {
          reads[i].push_back(j);
          readers[j].push_back(i);
        }
      }
    }
    // the finishing order over what each cell reads, then groups over who
    // reads it, latest finished first: that gives the groups nothing reads
    // first, so the list turns over at the end
    std::vector<std::size_t> finished;
    std::vector<bool> seen(count, false);
    for (std::size_t start = 0; start < count; ++start) {
      if (seen[start]) {
        continue;
      }
      std::vector<std::pair<std::size_t, std::size_t>> stack{{start, 0}};
      seen[start] = true;
      while (!stack.empty()) {
        auto &[i, next] = stack.back();
        if (next < reads[i].size()) {
          const std::size_t j = reads[i][next++];
          if (!seen[j]) {
            seen[j] = true;
            stack.emplace_back(j, 0);
          }
        } else {
          finished.push_back(i);
          stack.pop_back();
        }
      }
    }
    std::vector<std::vector<SheetPosition>> result;
    std::vector<bool> placed(count, false);
    for (const std::size_t start : std::views::reverse(finished)) {
      if (placed[start]) {
        continue;
      }
      std::vector<SheetPosition> group;
      std::vector<std::size_t> stack{start};
      placed[start] = true;
      while (!stack.empty()) {
        const std::size_t i = stack.back();
        stack.pop_back();
        group.push_back(cells[i]);
        for (const std::size_t j : readers[i]) {
          if (!placed[j]) {
            placed[j] = true;
            stack.push_back(j);
          }
        }
      }
      std::ranges::sort(group);
      result.push_back(std::move(group));
    }
    std::ranges::reverse(result);
    return result;
  }

  /// One group of @p cycle cells, swept as @p iteration states.
  void iterate(const std::vector<SheetPosition> &cycle,
               const formula::Iteration &iteration) const {
    bool converged = false;
    bool answered = true;
    for (std::uint32_t step = 0;
         step < iteration.steps && !converged && answered; ++step) {
      double change = 0;
      for (const SheetPosition &position : cycle) {
        // what reads a cell of the cycle reads this step's value
        forget();
        const Formula &entry = m_formulas->at(position);
        std::optional<formula::Value> value =
            entry.node.has_value() && !entry.spanned && !entry.array.has_value()
                ? formula::evaluate(*entry.node, position, *this, m_settings)
                : std::nullopt;
        if (!value.has_value()) {
          answered = false;
          break;
        }
        change =
            std::max(change, difference(m_iterated[position].value(), *value));
        m_iterated[position] = std::move(value);
      }
      converged = answered && change < iteration.difference;
    }
    // LibreOffice states `Err:523` for a cycle that does not converge, which
    // a file cannot hold, and Excel keeps what the last step computed
    const bool kept = converged || (answered && m_settings.dialect ==
                                                    formula::Dialect::excel);
    for (const SheetPosition &position : cycle) {
      if (kept) {
        m_circular.erase(position);
      } else {
        m_iterated[position] = std::nullopt;
      }
    }
  }

  /// How far apart two steps of a cycle leave a cell: infinite where they
  /// change what it is.
  [[nodiscard]] static double difference(const formula::Value &before,
                                         const formula::Value &after) {
    if (before.holds<double>() && after.holds<double>()) {
      return std::abs(after.get<double>() - before.get<double>());
    }
    return before == after ? 0 : std::numeric_limits<double>::infinity();
  }

  /// Drops every result computed so far, so each is computed again.
  void forget() const {
    m_results.clear();
    m_stack.clear();
    m_ancestors.clear();
    m_frontier_path.clear();
    m_frontier.reset();
  }

  /// Notes what @p result puts in each cell of @p span past its first, and
  /// gives what it puts in the first.
  std::optional<formula::Value>
  fill(const Span &span, const std::optional<formula::Matrix> &result) const {
    if (m_frontier.has_value()) {
      return std::nullopt; // cut short, so computed again
    }
    for_each_position(span, [&](const SheetPosition &position) {
      if (position != span.first) {
        m_results.emplace(
            position,
            result.has_value()
                ? std::optional(formula::spread(
                      *result, position.cell.column - span.first.cell.column,
                      position.cell.row - span.first.cell.row))
                : std::nullopt);
      }
    });
    return result.has_value() ? std::optional(formula::spread(*result, 0, 0))
                              : std::nullopt;
  }

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
  formula::Settings m_settings;
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
  /// The values a cycle's cells took, which the document's settings computed.
  mutable std::unordered_map<SheetPosition, std::optional<formula::Value>>
      m_iterated;
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
  // the formulas that stand for more than their own cell: a repeat, which
  // the evaluator does not compute apart, and an array formula's range
  std::vector<Span> spans;
  std::size_t spanned_positions = 0;
  for (std::uint32_t index = 0; index < source.sheets().size(); ++index) {
    const ElementIdentifier sheet_id = source.sheets()[index].identifier();
    sheet_index.emplace(sheet_id, index);
    adapter->sheet_adapter(sheet_id)->sheet_visit_formulas(
        sheet_id, [&](const std::uint32_t column, const std::uint32_t row,
                      const TableDimensions &span, const bool array,
                      const std::string &text) {
          constexpr auto max = std::numeric_limits<std::uint32_t>::max();
          if (span.rows > max - row || span.columns > max - column) {
            throw UnsupportedOperation();
          }
          const SheetPosition position(index, column, row);
          const bool spanned = array || span.rows > 1 || span.columns > 1;
          formulas[position] =
              Formula{sheet_id, formula::parse(text, *syntax), spanned};
          if (spanned) {
            const std::uint64_t count = std::uint64_t{span.rows} * span.columns;
            if (count > span_limit - spanned_positions) {
              throw UnsupportedOperation();
            }
            spanned_positions += static_cast<std::size_t>(count);
            spans.push_back(Span{position, span, array});
          }
        });
  }
  for (const Span &span : spans) {
    const ElementIdentifier sheet_id = formulas.at(span.first).sheet_id;
    std::optional<formula::Node> node = std::move(formulas.at(span.first).node);
    for_each_position(span, [&](const SheetPosition &position) {
      formulas[position] =
          span.array
              ? Formula{sheet_id, position == span.first ? node : std::nullopt,
                        false, position != span.first, span}
              : Formula{sheet_id, std::nullopt, true, false, std::nullopt};
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
  const auto compute_all = [&] {
    computed.clear();
    computed.reserve(ordered.size());
    for (const SheetPosition &position : ordered) {
      computed.push_back(
          Computed{position, source.cell(position), cells.compute(position)});
      // the cells an array formula fills, which its own cell computed
      if (const Formula &formula = formulas.at(position);
          formula.array.has_value()) {
        for_each_position(*formula.array, [&](const SheetPosition &output) {
          if (output != position) {
            computed.push_back(
                Computed{output, source.cell(output), cells.cell(output)});
          }
        });
      }
    }
  };
  compute_all();
  // a cycle computes where the document's settings say so, and then what
  // reads it
  if (cells.iterate()) {
    compute_all();
  }

  for (const auto &[position, before, value] : computed) {
    const Formula &formula = formulas.at(position);
    const ElementIdentifier sheet_id = formula.sheet_id;
    const abstract::SheetAdapter *sheet = adapter->sheet_adapter(sheet_id);
    const auto set = [&](const CellValue &cell_value) {
      if (formula.output) {
        sheet->sheet_set_array_result(sheet_id, position.cell.column,
                                      position.cell.row, cell_value);
      } else {
        sheet->sheet_set_result(sheet_id, position.cell.column,
                                position.cell.row, cell_value);
      }
    };
    if (!value.has_value()) {
      const SheetPosition &owner = formula.output && formula.array.has_value()
                                       ? formula.array->first
                                       : position;
      (cells.is_circular(owner) ? result.circular : result.unevaluated)
          .push_back(position);
      // the result the file caches is stale, where the file can drop it
      try {
        set(CellValue(ValueType::unknown));
      } catch (const UnsupportedOperation &) {
        continue;
      }
      continue;
    }
    try {
      set(cell_value_of(*value));
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
