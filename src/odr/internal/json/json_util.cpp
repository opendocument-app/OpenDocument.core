#include <odr/internal/json/json_util.hpp>

#include <odr/exceptions.hpp>

#include <nlohmann/json.hpp>

namespace odr::internal {

void json::check_json_file(const std::string_view text) {
  if (!nlohmann::json::accept(text)) {
    throw NoJsonFile();
  }
}

} // namespace odr::internal
