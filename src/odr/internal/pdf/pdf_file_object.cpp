#include <odr/internal/pdf/pdf_file_object.hpp>

#include <odr/internal/pdf/pdf_object_parser.hpp>

namespace odr::internal::pdf {

const ObjectReference &Trailer::root_reference() const {
  return dictionary["Root"].as_reference();
}

void Xref::append(const Xref &xref) {
  for (const auto &[reference, entry] : xref.table) {
    const auto next = table.lower_bound(ObjectReference(reference.id, 0));
    if (next == table.end() || next->first.id != reference.id) {
      table.emplace_hint(next, reference, entry);
    }
  }
}

void Xref::merge_hybrid(const Xref &xref_stream) {
  if (this == &xref_stream) {
    return;
  }
  for (const auto &[reference, entry] : xref_stream.table) {
    const auto first = table.lower_bound(ObjectReference(reference.id, 0));
    auto last = first;
    while (last != table.end() && last->first.id == reference.id &&
           last->second.is_free()) {
      ++last;
    }
    if (last != table.end() && last->first.id == reference.id) {
      continue;
    }
    table.erase(first, last);
    table.emplace_hint(last, reference, entry);
  }
}

} // namespace odr::internal::pdf
