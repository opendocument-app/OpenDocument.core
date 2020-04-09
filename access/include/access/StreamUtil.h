#ifndef ODR_ACCESS_STREAMUTIL_H
#define ODR_ACCESS_STREAMUTIL_H

#include <access/Stream.h>
#include <string>

namespace odr {
namespace access {

class StringSource final : public Source {
public:
  explicit StringSource(std::string &&data) : data(data), pos(0) {}

  std::uint32_t read(char *data, std::uint32_t amount) final;
  std::uint32_t available() const final;

private:
  const std::string data;
  std::size_t pos;
};

namespace StreamUtil {

extern std::string read(Source &);

extern void pipe(Source &, Sink &);

} // namespace StreamUtil

} // namespace access
} // namespace odr

#endif // ODR_ACCESS_STREAMUTIL_H
