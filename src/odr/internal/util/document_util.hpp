#pragma once

#include <odr/definitions.hpp>

namespace odr {
class DocumentPath;
}

namespace odr::internal::abstract {
class ElementAdapter;
}

namespace odr::internal::util::document {

/// The path from @p from_element_id (`null_element_id` for the root) down to
/// @p to_element_id; throws if the latter is not a descendant of the former.
DocumentPath extract_path(const abstract::ElementAdapter &element_adapter,
                          ElementIdentifier to_element_id,
                          ElementIdentifier from_element_id);

ElementIdentifier navigate_path(const abstract::ElementAdapter &element_adapter,
                                ElementIdentifier from_element_id,
                                const DocumentPath &path);

} // namespace odr::internal::util::document
