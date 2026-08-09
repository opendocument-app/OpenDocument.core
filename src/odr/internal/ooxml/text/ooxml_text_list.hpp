#pragma once

#include <odr/definitions.hpp>

#include <odr/internal/common/list_numbering.hpp>

#include <string>
#include <unordered_map>

#include <pugixml.hpp>

namespace odr::internal::ooxml::text {
class ElementRegistry;

/// @brief `word/numbering.xml`: the list level definitions, indexed.
class NumberingRegistry final {
public:
  NumberingRegistry() = default;
  NumberingRegistry(pugi::xml_node numbering_root, pugi::xml_node styles_root);

  /// The definition `num_id` gives `level` (counted from 0), already resolved
  /// through the abstract numbering and any override.
  [[nodiscard]] ListLevel level(const std::string &num_id,
                                std::uint32_t level) const;

private:
  std::unordered_map<std::string, pugi::xml_node> m_abstract_numbering;
  std::unordered_map<std::string, pugi::xml_node> m_numbering;
  std::unordered_map<std::string, std::string> m_style_numbering;

  [[nodiscard]] pugi::xml_node abstract_numbering_(const std::string &num_id,
                                                   std::uint32_t depth) const;
};

/// Stamps every list and item, in document order — which is what the counters
/// need.
void resolve_list_numbering(ElementRegistry &registry,
                            const NumberingRegistry &numbering,
                            ElementIdentifier root_id);

} // namespace odr::internal::ooxml::text
