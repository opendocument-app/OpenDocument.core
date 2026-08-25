#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::iwork {
class Budget;
class ElementRegistry;
class Message;

/// Appends the paragraphs of a `TSWP.StorageArchive` to @p parent_id. Shared
/// by every place text lives: a Pages body and a Keynote text box.
void parse_storage(ElementRegistry &registry, Budget &budget,
                   ElementIdentifier parent_id, const Message &storage);

} // namespace odr::internal::iwork
