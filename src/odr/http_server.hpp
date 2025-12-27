#pragma once

#include <odr/html.hpp>
#include <odr/logger.hpp>

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
  constexpr static auto prefix_pattern = R"(([a-zA-Z0-9_-]+))";

  struct Config {
    // TODO remove
    std::string cache_path{"/tmp/odr"};
  };

  explicit HttpServer(const Config &config,
                      std::shared_ptr<Logger> logger = Logger::create_null());

  [[nodiscard]] const Config &config() const;

  void connect_service(HtmlService service, const std::string &prefix) const;

  // TODO remove
  [[nodiscard]] HtmlViews serve_file(const DecodedFile &file,
                                     const std::string &prefix,
                                     const HtmlConfig &config) const;

  void listen(const std::string &host, std::uint32_t port) const;

  void clear() const;

  void stop() const;

private:
  class Impl;

  std::shared_ptr<Impl> m_impl;
};

} // namespace odr
