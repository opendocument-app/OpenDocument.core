#include <httplib/httplib.h>
#include <odr/server.h>

namespace odr {

class Server::Impl final {
public:
  explicit Impl(const ServerConfig &config) : m_config{config} {}

  void serve() {
    m_server.Get("/", [](const httplib::Request &, httplib::Response &res) {
      res.set_content("Hello World!", "text/plain");
    });

    m_server.listen(m_config.host, m_config.port);
  }

private:
  ServerConfig m_config;

  httplib::Server m_server;
};

Server::Server(const ServerConfig &config)
    : m_impl{std::make_unique<Impl>(config)} {}

void Server::serve() { m_impl->serve(); }

} // namespace odr
