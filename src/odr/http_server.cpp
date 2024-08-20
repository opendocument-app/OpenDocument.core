#include <odr/http_server.hpp>

#include <odr/file.hpp>
#include <odr/filesystem.hpp>

#include <random>

#include <httplib/httplib.h>

namespace odr {

class HttpServer::Impl {
public:
  explicit Impl(const HttpServer::Config &config) : m_config{config} {}

  [[nodiscard]] std::string host_file(File file) {
    std::string id = get_id();
    m_server.Get(
        "/" + id,
        [this, file = std::move(file)](const httplib::Request &req,
                                       httplib::Response &res) -> void {
          struct State {
            explicit State(std::unique_ptr<std::istream> stream_,
                           std::size_t buffer_size)
                : stream{std::move(stream_)}, buffer(buffer_size, 0) {}

            std::unique_ptr<std::istream> stream;
            std::vector<char> buffer;
          };

          auto state =
              std::make_shared<State>(file.stream(), m_config.buffer_size);
          httplib::ContentProvider content_provider =
              [state](std::size_t offset, std::size_t length,
                      httplib::DataSink &sink) -> bool {
            std::istream &stream = *state->stream;

            stream.seekg(static_cast<std::streamsize>(offset), std::ios::beg);
            stream.read(state->buffer.data(),
                        static_cast<std::streamsize>(length));
            sink.write(state->buffer.data(),
                       static_cast<std::size_t>(stream.gcount()));

            return !stream.eof();
          };

          res.set_content_provider(file.size(), "application/octet-stream",
                                   content_provider);
        });
    return id;
  }

  [[nodiscard]] std::string host_file(DecodedFile file) {
    std::string id = get_id();
    m_server.Get("/" + id, [file = std::move(file)](const httplib::Request &req,
                                                    httplib::Response &res) {
      res.set_content("Hello World!", "text/plain");
    });
    return id;
  }

  [[nodiscard]] std::string host_filesystem(Filesystem filesystem) {
    std::string id = get_id();
    m_server.Get("/" + id,
                 [filesystem = std::move(filesystem)](
                     const httplib::Request &req, httplib::Response &res) {
                   res.set_content("Hello World!", "text/plain");
                 });
    return id;
  }

  void listen(const std::string &host, std::uint32_t port) {
    m_server.listen(host, static_cast<int>(port));
  }

private:
  [[nodiscard]] static std::string get_id() {
    std::default_random_engine rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 9);

    std::string id(10, '0');
    for (char &c : id) {
      c = static_cast<char>('0' + dist(rng));
    }

    return id;
  }

  HttpServer::Config m_config;

  httplib::Server m_server;
};

HttpServer::HttpServer(const Config &config)
    : m_impl{std::make_unique<Impl>(config)} {}

std::string HttpServer::host_file(File file) {
  return m_impl->host_file(std::move(file));
}

std::string HttpServer::host_file(DecodedFile file) {
  return m_impl->host_file(std::move(file));
}

std::string HttpServer::host_filesystem(Filesystem filesystem) {
  return m_impl->host_filesystem(std::move(filesystem));
}

void HttpServer::listen(const std::string &host, std::uint32_t port) {
  m_impl->listen(host, port);
}

} // namespace odr
