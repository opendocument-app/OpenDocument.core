#include <odr/internal/json/json_util.hpp>

#include <nlohmann/json.hpp>

namespace odr::internal {

void json::check_json_file(std::istream &in) {
  // TODO limit check size
  const auto json = nlohmann::json::parse(in);
  (void)json;
  // TODO check if that even works
}

} // namespace odr::internal
