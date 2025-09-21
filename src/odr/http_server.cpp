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
  Impl(Config config, std::shared_ptr<Logger> logger)
      : m_config{std::move(config)}, m_logger{std::move(logger)} {
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

  const Config &config() const { return m_config; }

  const std::shared_ptr<Logger> &logger() const { return m_logger; }

  void serve_file(const httplib::Request &req, httplib::Response &res) {
    std::string id = req.matches[1].str();
    std::string path = req.matches.size() > 1 ? req.matches[2].str() : "";

    std::unique_lock lock{m_mutex};
    auto it = m_content.find(id);
    if (it == m_content.end()) {
      ODR_ERROR(*m_logger, "Content not found for ID: " << id);
      res.status = 404;
      return;
    }
    auto [_, service] = it->second;
    lock.unlock();

    serve_file(res, service, path);
  }

  void serve_file(httplib::Response &res, const HtmlService &service,
                  const std::string &path) const {
    if (!service.exists(path)) {
      ODR_ERROR(*m_logger, "File not found: " << path);
      res.status = 404;
      return;
    }

    ODR_VERBOSE(*m_logger, "Serving file: " << path);

    httplib::ContentProviderWithoutLength content_provider =
        [service = service, path = path](const std::size_t offset,
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
    ODR_VERBOSE(*m_logger, "Connecting service with prefix: " << prefix);

    std::unique_lock lock{m_mutex};

    if (m_content.contains(prefix)) {
      throw PrefixInUse(prefix);
    }

    m_content.emplace(prefix, Content{prefix, std::move(service)});
  }

  void listen(const std::string &host, const std::uint32_t port) {
    ODR_VERBOSE(*m_logger, "Listening on " << host << ":" << port);

    m_server.listen(host, static_cast<int>(port));
  }

  void clear() {
    ODR_VERBOSE(*m_logger, "Clearing HTTP server cache...");

    std::unique_lock lock{m_mutex};

    m_content.clear();

    for (const auto &entry :
         std::filesystem::directory_iterator(m_config.cache_path)) {
      std::filesystem::remove_all(entry.path());
    }
  }

  void stop() {
    ODR_VERBOSE(*m_logger, "Stopping HTTP server...");

    clear();

    m_server.stop();
  }

private:
  Config m_config;

  std::shared_ptr<Logger> m_logger;

  httplib::Server m_server;

  struct Content {
    std::string id;
    HtmlService service;
  };

  std::mutex m_mutex;
  std::unordered_map<std::string, Content> m_content;
};

HttpServer::HttpServer(const Config &config, std::shared_ptr<Logger> logger)
    : m_impl{std::make_unique<Impl>(config, std::move(logger))} {}

const HttpServer::Config &HttpServer::config() const {
  return m_impl->config();
}

void HttpServer::connect_service(HtmlService service,
                                 const std::string &prefix) const {
  static std::regex prefix_regex(prefix_pattern);
  if (!std::regex_match(prefix, prefix_regex)) {
    throw InvalidPrefix(prefix);
  }

  if (service.config().relative_resource_paths) {
    throw UnsupportedOption(
        "relative_resource_paths cannot be enabled in server mode");
  }
  if (!service.config().embed_shipped_resources) {
    throw UnsupportedOption(
        "embed_shipped_resources must be enabled in server mode");
  }

  m_impl->connect_service(std::move(service), prefix);
}

HtmlViews HttpServer::serve_file(const DecodedFile &file,
                                 const std::string &prefix,
                                 const HtmlConfig &config) const {
  const std::string cache_path = m_impl->config().cache_path + "/" + prefix;
  std::filesystem::create_directories(cache_path);

  const HtmlService service =
      html::translate(file, cache_path, config, m_impl->logger());

  connect_service(service, prefix);

  return service.list_views();
}

void HttpServer::listen(const std::string &host,
                        const std::uint32_t port) const {
  m_impl->listen(host, port);
}

void HttpServer::clear() const { m_impl->clear(); }

void HttpServer::stop() const { m_impl->stop(); }

} // namespace odr
