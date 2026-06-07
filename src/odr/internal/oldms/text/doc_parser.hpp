#pragma once

#include <odr/definitions.hpp>

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::oldms::text {
class ElementRegistry;

ElementIdentifier parse_tree(ElementRegistry &registry,
                             const abstract::ReadableFilesystem &files);

} // namespace odr::internal::oldms::text
