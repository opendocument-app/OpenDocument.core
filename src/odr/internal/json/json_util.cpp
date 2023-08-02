#include <nlohmann/json.hpp>
#include <odr/internal/json/json_util.h>

namespace odr::internal {

void json::check_json_file(std::istream &in) {
  // TODO limit check size
  nlohmann::json::parse(in);
  // TODO check if that even works
}

} // namespace odr::internal
