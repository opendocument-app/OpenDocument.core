#ifndef ODR_SERVER_H
#define ODR_SERVER_H

#include <memory>
#include <string>

namespace odr {

struct ServerConfig {
  std::string tmpDir;
};

class Server {
public:
  explicit Server(const ServerConfig &config);
  ~Server();

  void listen(const std::string &endpoint, int port) const;

private:
  class Impl;
  const std::unique_ptr<Impl> impl_;
};

} // namespace odr

#endif // ODR_SERVER_H
