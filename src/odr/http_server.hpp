#pragma once

#include <odr/html_service.hpp>

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
    std::string cache_path{"/tmp/odr"};
  };

  explicit HttpServer(const Config &config);

  const Config &config() const;

  void connect_service(HtmlService service, const std::string &prefix);

  HtmlViews serve_file(DecodedFile file, const std::string &prefix,
                       const HtmlConfig &config);

  void listen(const std::string &host, std::uint32_t port);

  void clear();

  void stop();

private:
  class Impl;

  std::shared_ptr<Impl> m_impl;
};

} // namespace odr
