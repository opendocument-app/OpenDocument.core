#include <odr/http_server.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <httplib/httplib.h>

#include <atomic>
#include <sstream>

namespace odr {

class HttpServer::Impl {
public:
  explicit Impl(std::shared_ptr<Logger> logger)
      : m_logger{std::move(logger)},
        m_server{std::make_unique<httplib::Server>()} {
    // Set up exception handler to catch any internal httplib exceptions.
    // This prevents crashes when exceptions occur during request processing.
    m_server->set_exception_handler([this](const httplib::Request & /*req*/,
                                           httplib::Response &res,
                                           const std::exception_ptr &ep) {
      try {
        if (ep != nullptr) {
          std::rethrow_exception(ep);
        }
      } catch (const std::exception &e) {
        ODR_ERROR(*m_logger, "Exception in HTTP handler: " << e.what());
      } catch (...) {
        ODR_ERROR(*m_logger, "Unknown exception in HTTP handler");
      }
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    });

    m_server->Get("/",
                  [](const httplib::Request & /*req*/, httplib::Response &res) {
                    res.set_content("Hello World!", "text/plain");
                  });

    m_server->Get("/file/" + std::string(prefix_pattern),
                  [this](const httplib::Request &req, httplib::Response &res) {
                    if (m_stopping.load(std::memory_order_acquire)) {
                      res.status = 503;
                      res.set_content("Service Unavailable", "text/plain");
                      return;
                    }
                    serve_file(req, res);
                  });
    m_server->Get("/file/" + std::string(prefix_pattern) + "/(.*)",
                  [this](const httplib::Request &req, httplib::Response &res) {
                    if (m_stopping.load(std::memory_order_acquire)) {
                      res.status = 503;
                      res.set_content("Service Unavailable", "text/plain");
                      return;
                    }
                    serve_file(req, res);
                  });
  }

  ~Impl() {
    // Ensure the server is properly stopped before destruction.
    // This prevents crashes from worker threads accessing freed memory.
    //
    // IMPORTANT: We must fully stop and destroy the server BEFORE
    // m_content is destroyed. httplib's thread pool threads may still
    // be running after stop() returns - they only fully stop when the
    // Server destructor joins them. By explicitly destroying the server
    // here (via unique_ptr::reset), we ensure all threads are joined
    // before any other members are destroyed.
    m_stopping.store(true, std::memory_order_release);
    if (m_server != nullptr) {
      m_server->stop();
      m_server.reset(); // Destroy server, join all thread pool threads
    }
    // Now safe to let other members destruct - no threads are running
  }

  // Prevent copying - the lambdas capture 'this' so copying would be unsafe
  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;

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

  std::uint32_t bind(const std::string &host, const std::uint32_t port,
                     const Options &options) {
    if (m_server == nullptr) {
      throw ServerNotBound();
    }
    // binding twice would overwrite the socket cpp-httplib holds, leaking the
    // first one and its port for good
    if (m_bound.load(std::memory_order_acquire)) {
      throw ServerAlreadyBound();
    }

#ifdef _WIN32
    // Windows keeps cpp-httplib's defaults, which set SO_EXCLUSIVEADDRUSE
    // alongside SO_REUSEADDR. The two flags mean the opposite of what they do
    // below: there SO_REUSEADDR lets a second live socket take the endpoint
    // over, and SO_EXCLUSIVEADDRUSE is what keeps it ours. Replacing that with
    // the posix mapping would hand the port away, so Options does not apply.
    static_cast<void>(options);
#else
    // cpp-httplib's default sets SO_REUSEPORT where it exists and SO_REUSEADDR
    // only otherwise, which is the wrong way round for a server that gets
    // restarted: only SO_REUSEADDR lets a port held by TIME_WAIT sockets be
    // bound again, while SO_REUSEPORT hands a second server a share of the
    // connections instead.
    // socket_t is not in the httplib namespace in every version, hence auto
    m_server->set_socket_options([options](const auto sock) {
      constexpr int yes = 1;
      const auto *const value = reinterpret_cast<const char *>(&yes);

      if (options.reuse_address) {
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, value, sizeof(yes));
      }
#ifdef SO_REUSEPORT
      if (options.reuse_port) {
        setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, value, sizeof(yes));
      }
#endif
    });
#endif

    const int bound =
        port == 0 ? m_server->bind_to_any_port(host)
                  : (m_server->bind_to_port(host, static_cast<int>(port))
                         ? static_cast<int>(port)
                         : -1);
    if (bound < 0) {
      throw ServerBindFailed(host, port);
    }

    m_bound.store(true, std::memory_order_release);

    ODR_VERBOSE(*m_logger, "Bound to " << host << ":" << bound);

    return static_cast<std::uint32_t>(bound);
  }

  void listen() const {
    // cpp-httplib's listen_after_bind() reports success for a server that was
    // never bound, so the state has to be checked here
    if (m_server == nullptr || !m_bound.load(std::memory_order_acquire)) {
      throw ServerNotBound();
    }

    ODR_VERBOSE(*m_logger, "Serving...");

    m_server->listen_after_bind();
  }

  void clear() {
    ODR_VERBOSE(*m_logger, "Dropping connected services...");

    std::unique_lock lock{m_mutex};

    m_content.clear();
  }

  void stop() {
    ODR_VERBOSE(*m_logger, "Stopping HTTP server...");

    // Set stopping flag first to reject new requests immediately.
    // This prevents new requests from starting while we're shutting down.
    m_stopping.store(true, std::memory_order_release);

    if (m_server != nullptr) {
      // Stop the server to prevent new connections.
      // Note: httplib::Server::stop() signals shutdown but thread pool
      // threads may still be running. They only fully stop when the
      // Server is destroyed. For explicit stop() calls (not destructor),
      // we destroy the server here to ensure threads are joined.
      m_server->stop();
      m_server.reset(); // Destroy server, join all thread pool threads
    }

    m_bound.store(false, std::memory_order_release);

    // Clear content after server is fully destroyed to avoid use-after-free.
    clear();
  }

private:
  std::shared_ptr<Logger> m_logger;

  // Flag to indicate server is shutting down - checked by handlers
  // to reject new requests during shutdown.
  std::atomic<bool> m_stopping{false};

  // Whether bind() has taken a socket. listen() needs it because cpp-httplib
  // will happily "serve" a server that never bound one.
  std::atomic<bool> m_bound{false};

  struct Content {
    std::string id;
    HtmlService service;
  };

  std::mutex m_mutex;
  std::unordered_map<std::string, Content> m_content;

  // IMPORTANT: m_server is declared LAST and as unique_ptr so we can
  // explicitly destroy it in the destructor BEFORE other members.
  // httplib's Server destructor joins thread pool threads, so we must
  // ensure threads are fully stopped before m_content is destroyed.
  // Using unique_ptr allows us to call reset() to trigger destruction
  // at a controlled point in the destructor.
  std::unique_ptr<httplib::Server> m_server;
};

HttpServer::HttpServer(const Config & /*config*/,
                       std::shared_ptr<Logger> logger)
    : m_impl{std::make_unique<Impl>(std::move(logger))} {}

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

std::uint32_t HttpServer::bind(const std::string &host,
                               const std::uint32_t port) const {
  return bind(host, port, Options{});
}

std::uint32_t HttpServer::bind(const std::string &host,
                               const std::uint32_t port,
                               const Options &options) const {
  return m_impl->bind(host, port, options);
}

void HttpServer::listen() const { m_impl->listen(); }

void HttpServer::clear() const { m_impl->clear(); }

void HttpServer::stop() const { m_impl->stop(); }

} // namespace odr
