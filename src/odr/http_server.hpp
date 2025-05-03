#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace odr {
class File;
class DecodedFile;
class Filesystem;
struct HtmlConfig;
class HtmlService;

class HttpServer {
public:
  struct Config {
    std::size_t buffer_size{4096};
    std::string output_path{"/tmp"};
  };

  explicit HttpServer(const Config &config);

  void connect_service(HtmlService service, const std::string &prefix);

  void serve_file(DecodedFile file, const std::string &prefix,
                  const HtmlConfig &config);

  void listen(const std::string &host, std::uint32_t port);

  void stop();

private:
  class Impl;

  std::shared_ptr<Impl> m_impl;
};

} // namespace odr
