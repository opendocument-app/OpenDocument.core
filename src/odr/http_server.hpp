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
  constexpr static const char *prefix_pattern = R"(([a-zA-Z0-9_-]+))";

  struct Config {
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
