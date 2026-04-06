#include <odr/internal/oldms/text/doc_parser.hpp>

#include <odr/internal/abstract/filesystem.hpp>

namespace odr::internal::oldms {

ElementIdentifier text::parse_tree(ElementRegistry &registry,
                                   const abstract::ReadableFilesystem &files) {
  return null_element_id;
}

} // namespace odr::internal::oldms
