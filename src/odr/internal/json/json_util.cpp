#include <nlohmann/json.hpp>
#include <odr/internal/json/json_util.hpp>

namespace odr::internal {

void json::check_json_file(std::istream &in) {
  // TODO limit check size
  auto _ = nlohmann::json::parse(in);
  (void)_;
  // TODO check if that even works
}

} // namespace odr::internal
