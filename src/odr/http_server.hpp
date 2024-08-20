#ifndef ODR_HTTP_SERVER_HPP
#define ODR_HTTP_SERVER_HPP

#include <cstdint>
#include <memory>
#include <string>

namespace odr {
class File;
class DecodedFile;
class Filesystem;

class HttpServer {
public:
  struct Config {
    std::size_t buffer_size{4096};
  };

  explicit HttpServer(const Config &config);

  std::string host_file(File file);
  std::string host_file(DecodedFile file);
  std::string host_filesystem(Filesystem filesystem);

  void listen(const std::string &host, std::uint32_t port);

private:
  class Impl;

  std::shared_ptr<Impl> m_impl;
};

} // namespace odr

#endif // ODR_HTTP_SERVER_HPP
