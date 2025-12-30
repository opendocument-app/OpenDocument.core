#pragma once

#include <odr/definitions.hpp>

namespace odr {
class DocumentPath;
}

namespace odr::internal::abstract {
class ElementAdapter;
}

namespace odr::internal::util::document {

DocumentPath extract_path(const abstract::ElementAdapter &element_adapter,
                          ElementIdentifier to_element_id,
                          ElementIdentifier from_element_id);

ElementIdentifier navigate_path(const abstract::ElementAdapter &element_adapter,
                                ElementIdentifier from_element_id,
                                const DocumentPath &path);

} // namespace odr::internal::util::document
