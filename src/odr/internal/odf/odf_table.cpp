#include <odr/internal/odf/odf_table.hpp>

#include <pugixml.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace odr::internal::odf {

namespace {

constexpr std::array row_group_names{
    std::string_view("table:table-header-rows"),
    std::string_view("table:table-rows"),
    std::string_view("table:table-row-group"),
};

constexpr std::array column_group_names{
    std::string_view("table:table-header-columns"),
    std::string_view("table:table-columns"),
    std::string_view("table:table-column-group"),
};

/// A group carries the visibility of what it holds ([ODF 1.2] 19.766).
bool is_displayed(const pugi::xml_node group) {
  const pugi::xml_attribute display = group.attribute("table:display");
  return !display || display.as_bool(true);
}

void collect(const pugi::xml_node parent, const std::string_view name,
             const std::span<const std::string_view> group_names,
             std::vector<pugi::xml_node> &out) {
  for (const pugi::xml_node child : parent.children()) {
    const std::string_view child_name = child.name();
    if (child_name == name) {
      out.push_back(child);
    } else if (std::ranges::find(group_names, child_name) !=
                   std::end(group_names) &&
               is_displayed(child)) {
      collect(child, name, group_names, out);
    }
  }
}

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::vector<pugi::xml_node> odf::table_rows(const pugi::xml_node table) {
  std::vector<pugi::xml_node> result;
  collect(table, "table:table-row", row_group_names, result);
  return result;
}

std::vector<pugi::xml_node> odf::table_columns(const pugi::xml_node table) {
  std::vector<pugi::xml_node> result;
  collect(table, "table:table-column", column_group_names, result);
  return result;
}

} // namespace odr::internal
