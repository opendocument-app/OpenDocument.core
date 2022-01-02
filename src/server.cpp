#include <httplib/httplib.h>
#include <odr/file.h>
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

  std::string add_file(const std::string &path) {
    return add_file(DecodedFile(path));
  }

  std::string add_file(const File &file) {
    return ""; // TODO
  }

  std::string add_file(const DecodedFile &file) {
    return ""; // TODO
  }

private:
  ServerConfig m_config;

  httplib::Server m_server;
};

Server::Server(const ServerConfig &config)
    : m_impl{std::make_unique<Impl>(config)} {}

void Server::serve() { m_impl->serve(); }

std::string Server::add_file(const std::string &path) {
  return m_impl->add_file(path);
}

std::string Server::add_file(const File &file) {
  return m_impl->add_file(file);
}

std::string Server::add_file(const DecodedFile &file) {
  return m_impl->add_file(file);
}

} // namespace odr
