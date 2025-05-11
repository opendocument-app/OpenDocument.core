#include <odr/http_server.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <httplib/httplib.h>

namespace odr {

class HttpServer::Impl {
public:
  explicit Impl(HttpServer::Config config) : m_config{std::move(config)} {
    m_server.Get("/",
                 [](const httplib::Request & /*req*/, httplib::Response &res) {
                   res.set_content("Hello World!", "text/plain");
                 });

    m_server.Get("/file/" + std::string(prefix_pattern),
                 [&](const httplib::Request &req, httplib::Response &res) {
                   serve_file(req, res);
                 });
    m_server.Get("/file/" + std::string(prefix_pattern) + "/(.*)",
                 [&](const httplib::Request &req, httplib::Response &res) {
                   serve_file(req, res);
                 });
  }

  const HttpServer::Config &config() const { return m_config; }

  void serve_file(const httplib::Request &req, httplib::Response &res) {
    std::string id = req.matches[1].str();
    std::string path = req.matches.size() > 1 ? req.matches[2].str() : "";

    std::unique_lock lock{m_mutex};
    auto it = m_content.find(id);
    if (it == m_content.end()) {
      res.status = 404;
      return;
    }
    Content content = it->second;
    lock.unlock();

    serve_file(res, content.service, path);
  }

  static void serve_file(httplib::Response &res, const HtmlService &service,
                         const std::string &path) {
    if (!service.exists(path)) {
      res.status = 404;
      return;
    }

    httplib::ContentProviderWithoutLength content_provider =
        [service = service, path = path](std::size_t offset,
                                         httplib::DataSink &sink) -> bool {
      if (offset != 0) {
        throw std::runtime_error("Invalid offset: " + std::to_string(offset) +
                                 ". Must be 0.");
      }
      service.write(path, sink.os);
      return false;
    };
    res.set_content_provider(service.mimetype(path), content_provider);
  }

  void connect_service(HtmlService service, const std::string &prefix) {
    std::unique_lock lock{m_mutex};

    if (m_content.contains(prefix)) {
      throw PrefixInUse(prefix);
    }

    m_content.emplace(prefix, Content{prefix, std::move(service)});
  }

  void listen(const std::string &host, std::uint32_t port) {
    m_server.listen(host, static_cast<int>(port));
  }

  void clear() {
    std::unique_lock lock{m_mutex};

    m_content.clear();

    for (const auto &entry :
         std::filesystem::directory_iterator(m_config.output_path)) {
      std::filesystem::remove_all(entry.path());
    }
  }

  void stop() {
    clear();

    m_server.stop();
  }

private:
  HttpServer::Config m_config;

  httplib::Server m_server;

  struct Content {
    std::string id;
    HtmlService service;
  };

  std::mutex m_mutex;
  std::unordered_map<std::string, Content> m_content;
};

HttpServer::HttpServer(const Config &config)
    : m_impl{std::make_unique<Impl>(config)} {}

const HttpServer::Config &HttpServer::config() const {
  return m_impl->config();
}

void HttpServer::connect_service(HtmlService service,
                                 const std::string &prefix) {
  m_impl->connect_service(std::move(service), prefix);
}

HtmlViews HttpServer::serve_file(DecodedFile file, const std::string &prefix,
                                 const HtmlConfig &config) {
  static std::regex prefix_regex(prefix_pattern);
  if (!std::regex_match(prefix, prefix_regex)) {
    throw InvalidPrefix(prefix);
  }

  if (config.relative_resource_paths) {
    throw UnsupportedOption(
        "relative_resource_paths cannot be enabled in server mode");
  }
  if (!config.embed_shipped_resources) {
    throw UnsupportedOption(
        "embed_shipped_resources must be enabled in server mode");
  }

  std::string output_path = m_impl->config().output_path + "/" + prefix;
  std::filesystem::create_directories(output_path);

  HtmlService service = html::translate(file, output_path, config);

  m_impl->connect_service(service, prefix);

  return service.list_views();
}

void HttpServer::listen(const std::string &host, std::uint32_t port) {
  m_impl->listen(host, port);
}

void HttpServer::clear() { m_impl->clear(); }

void HttpServer::stop() { m_impl->stop(); }

} // namespace odr
