#include <odr/internal/svg/svg_util.hpp>

#include <odr/internal/util/xml_util.hpp>

#include <pugixml.hpp>

#include <stdexcept>
#include <string_view>

namespace odr::internal {

void svg::check_svg_file(std::istream &in) {
  const pugi::xml_document document = util::xml::parse(in);

  // pugixml does not process namespaces, so the root element carries whatever
  // prefix the document bound to the svg namespace
  std::string_view name = document.document_element().name();
  if (const std::size_t colon = name.find(':');
      colon != std::string_view::npos) {
    name.remove_prefix(colon + 1);
  }

  if (name != "svg") {
    throw std::runtime_error("no svg file");
  }
}

} // namespace odr::internal
