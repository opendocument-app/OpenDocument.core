#include <odr/internal/pdf/pdf_file_object.hpp>

namespace odr::internal::pdf {

const ObjectReference &Trailer::root_reference() const {
  return dictionary["Root"].as_reference();
}

void Xref::append(const Xref &xref) {
  table.insert(std::begin(xref.table), std::end(xref.table));
}

} // namespace odr::internal::pdf
