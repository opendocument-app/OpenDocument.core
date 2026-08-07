#include <odr/http_server.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <httplib/httplib.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <sstream>

namespace odr {

class HttpServer::Impl : public std::enable_shared_from_this<Impl> {
public:
  /// What a HttpServer holds: a second reference whose deleter stops the server
  /// rather than destroying it. ~Impl cannot be that signal - listen() keeps a
  /// reference of its own, so the impl outlives the handles while it serves.
  static std::shared_ptr<Impl> create(const Logger &logger) {
    std::shared_ptr<Impl> owner = std::make_shared<Impl>(logger);
    Impl *const impl = owner.get();

    // shared_from_this() stays bound to the control block make_shared put
    // there, so listen() holds `owner`'s reference, not one of these
    return std::shared_ptr<Impl>{
        impl, [owner = std::move(owner)](Impl * /*owner already has it*/) {
          owner->stop();
        }};
  }

  explicit Impl(const Logger &logger)
      : m_logger{logger}, m_server{std::make_shared<httplib::Server>()} {
    // an exception escaping a handler tears down the process otherwise
    m_server->set_exception_handler([this](const httplib::Request & /*req*/,
                                           httplib::Response &res,
                                           const std::exception_ptr &ep) {
      try {
        if (ep != nullptr) {
          std::rethrow_exception(ep);
        }
      } catch (const std::exception &e) {
        ODR_ERROR(m_logger, "Exception in HTTP handler: " << e.what());
      } catch (...) {
        ODR_ERROR(m_logger, "Unknown exception in HTTP handler");
      }
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    });

    m_server->Get("/",
                  [](const httplib::Request & /*req*/, httplib::Response &res) {
                    res.set_content("Hello World!", "text/plain");
                  });

    const auto file_handler = [this](const httplib::Request &req,
                                     httplib::Response &res) {
      if (m_stopping.load(std::memory_order_acquire)) {
        res.status = 503;
        res.set_content("Service Unavailable", "text/plain");
        return;
      }
      serve_file(req, res);
    };
    m_server->Get("/file/" + std::string(prefix_pattern), file_handler);
    m_server->Get("/file/" + std::string(prefix_pattern) + "/(.*)",
                  file_handler);
  }

  ~Impl() {
    // cannot run underneath an accept loop - listen() holds a reference of its
    // own - but stop() is still what closes the socket and joins the pool,
    // before m_content is destroyed after this body
    stop();
  }

  // the handler lambdas capture `this`
  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;

  void serve_file(const httplib::Request &req, httplib::Response &res) {
    try {
      const std::string id = req.matches[1].str();
      // the route without a trailing path has the one group
      const std::string path =
          req.matches.size() > 2 ? req.matches[2].str() : "";

      std::unique_lock lock{m_mutex};
      const auto it = m_content.find(id);
      if (it == m_content.end()) {
        ODR_ERROR(m_logger, "Content not found for ID: " << id);
        res.status = 404;
        return;
      }
      const HtmlService service = it->second.service;
      lock.unlock();

      serve_file(res, service, path);
    } catch (const std::exception &e) {
      ODR_ERROR(m_logger, "Error handling request: " << e.what());
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    } catch (...) {
      ODR_ERROR(m_logger, "Unknown error handling request");
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    }
  }

  void serve_file(httplib::Response &res, const HtmlService &service,
                  const std::string &path) const {
    if (!service.exists(path)) {
      ODR_ERROR(m_logger, "File not found: " << path);
      res.status = 404;
      return;
    }

    ODR_VERBOSE(m_logger, "Serving file: " << path);

    // buffered rather than streamed: a chunked ContentProviderWithoutLength
    // crashes httplib::Server::write_response_core when the client disconnects,
    // the content generation throws, or the server stops mid-request
    try {
      std::ostringstream buffer;
      service.write(path, buffer);
      res.set_content(buffer.str(), service.mimetype(path));
    } catch (const std::exception &e) {
      ODR_ERROR(m_logger, "Error serving file " << path << ": " << e.what());
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    } catch (...) {
      ODR_ERROR(m_logger, "Unknown error serving file: " << path);
      res.status = 500;
      res.set_content("Internal Server Error", "text/plain");
    }
  }

  void connect_service(HtmlService service, const std::string &prefix) {
    ODR_VERBOSE(m_logger, "Connecting service with prefix: " << prefix);

    std::unique_lock lock{m_mutex};

    if (m_content.contains(prefix)) {
      throw PrefixInUse(prefix);
    }

    m_content.emplace(prefix, Content{std::move(service)});
  }

  std::uint32_t bind(const std::string &host, const std::uint32_t port,
                     const Options &options) {
    const std::unique_lock lock{m_mutex};

    if (m_server == nullptr) {
      throw ServerNotBound();
    }
    // binding twice would overwrite the socket cpp-httplib holds, leaking the
    // first one and its port for good
    if (m_bound) {
      throw ServerAlreadyBound();
    }

#ifdef _WIN32
    // Options does not apply: Windows keeps cpp-httplib's
    // SO_EXCLUSIVEADDRUSE defaults, where SO_REUSEADDR lets another live socket
    // take the endpoint over and the posix mapping below would hand the port
    // away
    static_cast<void>(options);
#else
    // cpp-httplib defaults to SO_REUSEPORT where it exists, the wrong way round
    // for a server that gets restarted: only SO_REUSEADDR rebinds a port held
    // by TIME_WAIT sockets, SO_REUSEPORT shares connections with a second
    // server instead.
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
      // a failed bind decommissions the server and only stop() clears that, so
      // without this no later bind - a fallback to any port - could succeed
      m_server->stop();
      throw ServerBindFailed(host, port);
    }

    m_bound = true;

    ODR_VERBOSE(m_logger, "Bound to " << host << ":" << bound);

    return static_cast<std::uint32_t>(bound);
  }

  void listen() {
    // this blocks on a thread of the caller's; a reference of its own keeps the
    // impl - server, mutex, condition variable - alive under the accept loop,
    // whatever the thread owning the HttpServer does meanwhile
    const std::shared_ptr<Impl> self = shared_from_this();

    std::shared_ptr<httplib::Server> server;

    {
      std::unique_lock lock{m_mutex};

      if (m_stopping.load(std::memory_order_acquire)) {
        // stop() got here first, so there is nothing left to serve
        return;
      }
      // cpp-httplib's listen_after_bind() reports success for a server that was
      // never bound, so the state has to be checked here
      if (m_server == nullptr || !m_bound) {
        throw ServerNotBound();
      }

      server = m_server;
      ++m_listening;
    }

    // right after the count went up, before anything that can throw: stop()
    // waits for the count and would never come back otherwise
    const ListenGuard guard{*this};

    ODR_VERBOSE(m_logger, "Serving...");

    server->listen_after_bind();
  }

  bool is_running() const {
    const std::unique_lock lock{m_mutex};

    return m_listening > 0;
  }

  void clear() {
    ODR_VERBOSE(m_logger, "Dropping connected services...");

    std::unique_lock lock{m_mutex};

    m_content.clear();
  }

  void stop() {
    // whoever gets here first takes the server away from the impl; a second
    // stop() finds nothing to close and only waits below
    std::shared_ptr<httplib::Server> server;

    {
      std::unique_lock lock{m_mutex};

      if (m_stopping.load(std::memory_order_acquire) && m_server == nullptr &&
          m_listening == 0) {
        // stopped already, and nothing left in flight - both the last handle
        // and ~Impl get here
        return;
      }

      ODR_VERBOSE(m_logger, "Stopping HTTP server...");

      // rejects requests in flight, and any listen() that has not started yet
      m_stopping.store(true, std::memory_order_release);
      m_bound = false;
      server = std::move(m_server);

      if (server != nullptr && m_listening > 0) {
        // httplib::Server::stop() only closes the listening socket once the
        // accept loop is up, and double-closes the descriptor if it runs again
        // after that, so a listen() on its way there has to be waited for -
        // httplib's own flag being the only thing to wait on
        while (m_listening > 0 && !server->is_running()) {
          m_listen_done.wait_for(lock, std::chrono::milliseconds{1});
        }
        if (server->is_running()) {
          lock.unlock();
          server->stop();
          lock.lock();
        }
      }

      // the accept loop stands on the server and on what the handlers capture,
      // so neither may go before listen() has returned
      m_listen_done.wait(lock, [this] { return m_listening == 0; });
    }

    // nothing is serving any more
    clear();

    // ~server here, which joins the thread pool
  }

private:
  /// Marks a listen() as done however it leaves, so stop() can wait for it.
  struct ListenGuard {
    Impl &impl;

    ~ListenGuard() {
      const std::unique_lock lock{impl.m_mutex};

      --impl.m_listening;
      // under the lock: the waiter may be tearing the impl down right after
      impl.m_listen_done.notify_all();
    }
  };

  Logger m_logger;

  // guards the lifecycle state below as well as m_content
  mutable std::mutex m_mutex;
  // signalled when a listen() returns
  std::condition_variable m_listen_done;
  // listen() calls in flight - 0 or 1 in any sane use
  std::size_t m_listening{0};

  // rejects new requests; atomic because the handlers read it without the lock
  std::atomic<bool> m_stopping{false};

  // whether bind() has taken a socket - cpp-httplib will happily "serve" a
  // server that never bound one
  bool m_bound{false};

  struct Content {
    HtmlService service;
  };

  std::unordered_map<std::string, Content> m_content;

  // Shared rather than unique: listen() takes a reference of its own, so the
  // server outlives an overlapping stop() and is destroyed - joining the thread
  // pool - only once the accept loop is off it.
  std::shared_ptr<httplib::Server> m_server;
};

HttpServer::HttpServer(const Config & /*config*/, const Logger &logger)
    : m_impl{Impl::create(logger)} {}

void HttpServer::connect_service(HtmlService service,
                                 const std::string &prefix) const {
  static std::regex prefix_regex(prefix_pattern);
  if (!std::regex_match(prefix, prefix_regex)) {
    throw InvalidPrefix(prefix);
  }

  m_impl->connect_service(std::move(service), prefix);
}

std::uint32_t HttpServer::bind(const std::string &host,
                               const std::uint32_t port,
                               const Options &options) const {
  return m_impl->bind(host, port, options);
}

void HttpServer::listen() const { m_impl->listen(); }

bool HttpServer::is_running() const { return m_impl->is_running(); }

void HttpServer::clear() const { m_impl->clear(); }

void HttpServer::stop() const { m_impl->stop(); }

} // namespace odr
