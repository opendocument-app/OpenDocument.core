#include <odr/http_server.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <httplib/httplib.h>

#include <sstream>

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
    try {
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
    } catch (const std::exception &e) {
      ODR_ERROR(*m_logger, "Error handling request: " << e.what());
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    } catch (...) {
      ODR_ERROR(*m_logger, "Unknown error handling request");
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    }
  }

  void serve_file(httplib::Response &res, const HtmlService &service,
                  const std::string &path) const {
    if (!service.exists(path)) {
      ODR_ERROR(*m_logger, "File not found: " << path);
      res.status = 404;
      return;
    }

    ODR_VERBOSE(*m_logger, "Serving file: " << path);

    // Buffer content to avoid streaming issues on Android.
    // Using ContentProviderWithoutLength (chunked transfer encoding) can cause
    // SIGSEGV crashes in httplib::Server::write_response_core when:
    // 1. The client disconnects during transfer
    // 2. Exceptions are thrown during content generation
    // 3. The server is stopped while requests are in-flight
    // By buffering content first, we can handle errors gracefully and use
    // Content-Length based responses which are more reliable.
    try {
      std::ostringstream buffer;
      service.write(path, buffer);
      res.set_content(buffer.str(), service.mimetype(path));
    } catch (const std::exception &e) {
      ODR_ERROR(*m_logger, "Error serving file " << path << ": " << e.what());
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    } catch (...) {
      ODR_ERROR(*m_logger, "Unknown error serving file: " << path);
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    }
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

    // Stop the server first to prevent new requests and allow in-flight
    // requests to complete. This fixes a race condition where clear() was
    // called before stop(), potentially invalidating resources while requests
    // were still being processed.
    m_server.stop();

    clear();
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
