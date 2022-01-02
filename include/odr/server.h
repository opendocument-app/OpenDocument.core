#ifndef ODR_SERVER_H
#define ODR_SERVER_H

#include <cstdint>
#include <memory>

namespace odr {

struct ServerConfig final {
  const char *host;
  std::int32_t port;
};

class Server final {
public:
  explicit Server(const ServerConfig &config);

  void serve();

private:
  class Impl;

  std::unique_ptr<Impl> m_impl;
};

} // namespace odr

#endif // ODR_SERVER_H
