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

void collect(const pugi::xml_node parent, const std::string_view name,
             const std::span<const std::string_view> group_names,
             std::vector<pugi::xml_node> &out) {
  for (const pugi::xml_node child : parent.children()) {
    const std::string_view child_name = child.name();
    if (child_name == name) {
      out.push_back(child);
    } else if (std::ranges::find(group_names, child_name) !=
               std::end(group_names)) {
      collect(child, name, group_names, out);
    }
  }
}

} // namespace

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

} // namespace odr::internal::odf
