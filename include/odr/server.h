#ifndef ODR_SERVER_H
#define ODR_SERVER_H

#include <cstdint>
#include <memory>

namespace odr {
class File;
class DecodedFile;

struct ServerConfig final {
  const char *host;
  std::int32_t port;

  // TODO temporary directory
};

class Server final {
public:
  explicit Server(const ServerConfig &config);

  void serve();

  std::string register_file(const std::string &path);
  std::string register_file(const File &file);
  std::string register_file(const DecodedFile &file);

private:
  class Impl;

  std::unique_ptr<Impl> m_impl;
};

} // namespace odr

#endif // ODR_SERVER_H
