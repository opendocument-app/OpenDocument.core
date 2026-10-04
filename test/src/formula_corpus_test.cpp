#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/sheet_cell_source.hpp>
#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_value.hpp>
#include <odr/internal/open_strategy.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;
using formula::Value;

namespace {

std::string spelled(const Value &value) {
  if (value.holds<double>()) {
    std::ostringstream out;
    out.precision(17);
    out << value.get<double>();
    return out.str();
  }
  if (value.holds<std::string>()) {
    return '"' + value.get<std::string>() + '"';
  }
  if (value.holds<bool>()) {
    return value.get<bool>() ? "TRUE" : "FALSE";
  }
  if (value.holds<formula::ErrorType>()) {
    return std::string(formula::to_string(value.get<formula::ErrorType>()));
  }
  return "(empty)";
}

/// Whether an answer is the result the file caches. A number is cached with
/// about 15 significant digits.
bool same(const Value &answer, const Value &cached) {
  if (answer.holds<double>() && cached.holds<double>()) {
    const double a = answer.get<double>();
    const double b = cached.get<double>();
    return a == b ||
           std::abs(a - b) <= 1e-12 * std::max(std::abs(a), std::abs(b));
  }
  return answer == cached;
}

struct Tally final {
  std::size_t formulas{0};
  std::size_t answered{0};
  std::vector<std::string> differences;
};

void evaluate_file(const TestFile &file, Tally &tally) {
  const std::shared_ptr<abstract::DecodedFile> decoded =
      open_strategy::open_file(std::make_shared<DiskFile>(file.absolute_path),
                               {}, Logger::null());
  const auto document_file =
      std::dynamic_pointer_cast<abstract::DocumentFile>(decoded);
  if (document_file == nullptr) {
    return;
  }
  const std::shared_ptr<abstract::Document> document =
      document_file->document();
  const std::optional<formula::Syntax> syntax =
      formula::syntax_of(document->file_type());
  const abstract::ElementAdapter *adapter = document->element_adapter();
  if (!syntax.has_value() || adapter == nullptr) {
    return;
  }
  const SheetCellSource source(*document);

  std::uint32_t index = 0;
  for (ElementIdentifier sheet =
           adapter->element_first_child(document->root_element());
       sheet != null_element_id; sheet = adapter->element_next_sibling(sheet)) {
    if (adapter->element_type(sheet) != ElementType::sheet) {
      continue;
    }
    adapter->sheet_adapter(sheet)->sheet_visit_formulas(
        sheet, [&](const std::uint32_t column, const std::uint32_t row,
                   const std::string &text) {
          ++tally.formulas;
          const SheetPosition position(index, column, row);
          const std::optional<formula::Node> node =
              formula::parse(text, *syntax);
          const std::optional<Value> cached = source.cell(position);
          if (!node.has_value() || !cached.has_value()) {
            return;
          }
          const std::optional<Value> answer =
              formula::evaluate(*node, position, source, source.settings());
          if (!answer.has_value()) {
            return;
          }
          ++tally.answered;
          if (!same(*answer, *cached)) {
            tally.differences.push_back(
                file.short_path + " " + position.to_string() + " " + text +
                ": " + spelled(*answer) + ", cached " + spelled(*cached));
          }
        });
    ++index;
  }
}

} // namespace

/// Decision 29 of `docs/design/spreadsheet-editing.md`: an answer is never
/// wrong. Every formula of the spreadsheets in the corpus is evaluated, and
/// each answer is compared with the result the file caches.
TEST(FormulaCorpus, every_answer_is_the_result_the_file_caches) {
  Tally tally;
  for (const FileType type : {FileType::opendocument_spreadsheet,
                              FileType::office_open_xml_workbook}) {
    for (const TestFile &file : TestData::test_files(type)) {
      if (file.password.has_value()) {
        continue;
      }
      evaluate_file(file, tally);
    }
  }

  std::cout << tally.answered << " of " << tally.formulas
            << " formulas answered, " << tally.differences.size()
            << " differ from the cached result" << std::endl;
  for (const std::string &difference : tally.differences) {
    ADD_FAILURE() << difference;
  }
}
