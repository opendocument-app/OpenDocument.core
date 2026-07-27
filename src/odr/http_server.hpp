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

/// Server-wide settings for HttpServer. Empty since cache_path went with
/// serve_file: what a service was translated into belongs to whoever
/// translated it.
struct HttpServerConfig {};

/// Socket options for HttpServer::bind(). POSIX only: Windows keeps
/// cpp-httplib's exclusive-address defaults, where these flags mean the
/// opposite.
struct HttpServerOptions {
  bool reuse_address{true}; ///< SO_REUSEADDR, so a port held only by sockets in
                            ///< TIME_WAIT can be bound again
  bool reuse_port{false};   ///< SO_REUSEPORT where the platform has it. Two
                            ///< live servers on one port share the incoming
                            ///< connections between them, hence off
};

class HttpServer {
public:
  constexpr static auto prefix_pattern = R"(([a-zA-Z0-9_-]+))";

  // at namespace scope, like HtmlConfig, so both can be defaulted with `= {}`
  // below: a nested class's member initializers are not parsed until the
  // enclosing class is complete, which a default argument here cannot wait for
  using Config = HttpServerConfig;
  using Options = HttpServerOptions;

  explicit HttpServer(const Config &config = {},
                      const Logger &logger = Logger::null());

  void connect_service(HtmlService service, const std::string &prefix) const;

  /// Binds the socket and returns the port it got - pass port 0 for any free
  /// one. Connections are accepted into the backlog from here on, before
  /// listen() runs. Throws ServerBindFailed / ServerAlreadyBound.
  std::uint32_t bind(const std::string &host, std::uint32_t port,
                     const Options &options = {}) const;

  /// Serves what bind() opened until stop(). Throws ServerNotBound.
  void listen() const;

  /// Drops the connected services. Files they were translated into are the
  /// caller's, and are left alone.
  void clear() const;

  /// Stops listen() and releases the socket. A server that was bound but never
  /// listened keeps its port until the process ends - cpp-httplib only closes
  /// the socket from its accept loop.
  void stop() const;

private:
  class Impl;

  std::shared_ptr<Impl> m_impl;
};

} // namespace odr
