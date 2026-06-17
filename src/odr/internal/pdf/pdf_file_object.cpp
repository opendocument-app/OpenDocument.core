#include <odr/internal/pdf/pdf_file_object.hpp>

#include <odr/internal/pdf/pdf_object_parser.hpp>

namespace odr::internal::pdf {

const ObjectReference &Trailer::root_reference() const {
  return dictionary["Root"].as_reference();
}

void Xref::append(const Xref &xref) {
  table.insert(std::begin(xref.table), std::end(xref.table));
}

void Xref::merge_hybrid(const Xref &xref_stream) {
  for (const auto &[reference, entry] : xref_stream.table) {
    bool absent_or_free = true;
    for (auto it = table.lower_bound(ObjectReference(reference.id, 0));
         it != std::end(table) && it->first.id == reference.id; ++it) {
      if (!it->second.is_free()) {
        absent_or_free = false;
        break;
      }
    }
    if (absent_or_free) {
      table.insert_or_assign(reference, entry);
    }
  }
}

} // namespace odr::internal::pdf
