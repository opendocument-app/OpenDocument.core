#include <odr/exceptions.hpp>
#include <odr/http_server.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

using namespace odr;
using namespace std::chrono_literals;

namespace {

/// Spins until listen() is in flight, so a test can stop a server that serves.
void wait_until_running(const HttpServer &server) {
  while (!server.is_running()) {
    std::this_thread::sleep_for(1ms);
  }
}

} // namespace

TEST(HttpServer, bind_reports_the_port_it_got) {
  const HttpServer server;

  const std::uint32_t port = server.bind("127.0.0.1", 0);
  EXPECT_NE(port, 0);

  server.stop();
}

TEST(HttpServer, bind_twice_is_refused) {
  const HttpServer server;

  server.bind("127.0.0.1", 0);
  // a second bind would replace the socket cpp-httplib holds, leaking the first
  // one and its port until the process ends
  EXPECT_THROW(server.bind("127.0.0.1", 0), ServerAlreadyBound);

  server.stop();
}

TEST(HttpServer, bind_reports_a_port_in_use) {
  const HttpServer taken;
  const std::uint32_t port = taken.bind("127.0.0.1", 0);

  const HttpServer other;
  // reuse_port defaults off, or the two would share the port
  EXPECT_THROW(other.bind("127.0.0.1", port), ServerBindFailed);

  taken.stop();
}

TEST(HttpServer, listen_without_bind_is_refused) {
  const HttpServer server;

  // cpp-httplib's listen_after_bind() reports success for this
  EXPECT_THROW(server.listen(), ServerNotBound);
}

TEST(HttpServer, stop_returns_only_once_listen_has) {
  const HttpServer server;
  server.bind("127.0.0.1", 0);

  std::atomic<bool> returned{false};
  std::thread thread{[&server, &returned] {
    server.listen();
    returned.store(true);
  }};

  wait_until_running(server);
  server.stop();

  // the accept loop is gone for good, so tearing the rest down is safe - which
  // is what dropping the server underneath it used to get wrong
  EXPECT_FALSE(server.is_running());

  thread.join();
  EXPECT_TRUE(returned.load());
}

TEST(HttpServer, stop_before_the_accept_loop_is_up_still_returns) {
  // deliberately no wait_until_running: this is the window where cpp-httplib
  // has not taken the socket over yet and its own stop() does nothing, so a
  // repeat is enough to hang or to close a descriptor twice
  for (int i = 0; i < 20; ++i) {
    const HttpServer server;
    server.bind("127.0.0.1", 0);

    std::thread thread{[&server] { server.listen(); }};

    server.stop();
    thread.join();
  }
}

TEST(HttpServer, stop_twice_is_harmless) {
  const HttpServer server;
  server.bind("127.0.0.1", 0);

  std::thread thread{[&server] { server.listen(); }};

  wait_until_running(server);
  // cpp-httplib's stop() asserts, then closes an already closed descriptor,
  // when it runs a second time on a server that is still winding down
  server.stop();
  server.stop();

  thread.join();
}

TEST(HttpServer, destroying_the_last_handle_stops_listen) {
  std::thread thread;

  {
    const HttpServer server;
    server.bind("127.0.0.1", 0);

    thread = std::thread{[&server] { server.listen(); }};
    wait_until_running(server);
  }

  // the destructor stopped the accept loop and waited for it, so this returns
  // rather than blocking forever
  thread.join();
}

TEST(HttpServer, listen_after_stop_returns) {
  const HttpServer server;
  server.bind("127.0.0.1", 0);
  server.stop();

  // a listen thread that starts late finds the server stopped; there is nothing
  // to serve, but nothing went wrong either
  EXPECT_NO_THROW(server.listen());
}
