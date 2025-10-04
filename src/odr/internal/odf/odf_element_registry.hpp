#pragma once

#include <odr/internal/common/document_element_registry.hpp>

#include <pugixml.hpp>

namespace odr::internal::odf {

class ElementRegistry : public common::ElementRegistry<ElementRegistry> {
public:
  using Base = common::ElementRegistry<ElementRegistry>;

  struct Entry final : Base::Entry {
    pugi::xml_node node;
  };
};

} // namespace odr::internal::odf
