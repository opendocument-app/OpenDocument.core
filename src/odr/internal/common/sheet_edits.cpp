#include <odr/internal/common/sheet_edits.hpp>

#include <odr/internal/common/sheet_dependencies.hpp>

namespace odr::internal {

SheetEdits::SheetEdits(const abstract::Document *document)
    : m_document{document} {}

SheetEdits::~SheetEdits() = default;

const SheetDependencies &SheetEdits::dependencies() {
  if (m_dependencies == nullptr) {
    m_dependencies =
        std::make_unique<SheetDependencies>(SheetDependencies::of(*m_document));
  }
  return *m_dependencies;
}

void SheetEdits::note_written(const ElementIdentifier sheet,
                              const TablePosition &position) {
  m_written.emplace_back(sheet, position);
}

void SheetEdits::note_formula_changed() noexcept { m_dependencies.reset(); }

void SheetEdits::note_moved() noexcept {
  m_dependencies.reset();
  m_moved = true;
}

const std::vector<std::pair<ElementIdentifier, TablePosition>> &
SheetEdits::written() const noexcept {
  return m_written;
}

bool SheetEdits::moved() const noexcept { return m_moved; }

void SheetEdits::forget() noexcept {
  m_written.clear();
  m_moved = false;
}

} // namespace odr::internal
