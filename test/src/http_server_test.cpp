#include <odr/exceptions.hpp>
#include <odr/http_server.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>

using namespace odr;

namespace {

HttpServer::Config test_config(const std::string &name) {
  HttpServer::Config config;
  config.cache_path = (std::filesystem::temp_directory_path() / name).string();
  std::filesystem::create_directories(config.cache_path);
  return config;
}

} // namespace

TEST(HttpServer, bind_reports_the_port_it_got) {
  const HttpServer server(test_config("odr-http-server-test-any-port"));

  const std::uint32_t port = server.bind("127.0.0.1", 0);
  EXPECT_NE(port, 0);

  server.stop();
}

TEST(HttpServer, bind_twice_is_refused) {
  const HttpServer server(test_config("odr-http-server-test-twice"));

  server.bind("127.0.0.1", 0);
  // a second bind would replace the socket cpp-httplib holds, leaking the first
  // one and its port until the process ends
  EXPECT_THROW(server.bind("127.0.0.1", 0), ServerAlreadyBound);

  server.stop();
}

TEST(HttpServer, bind_reports_a_port_in_use) {
  const HttpServer taken(test_config("odr-http-server-test-taken"));
  const std::uint32_t port = taken.bind("127.0.0.1", 0);

  const HttpServer other(test_config("odr-http-server-test-other"));
  HttpServer::Options options;
  options.reuse_port = false; // or the two would share the port
  EXPECT_THROW(other.bind("127.0.0.1", port, options), ServerBindFailed);

  taken.stop();
}

TEST(HttpServer, listen_without_bind_is_refused) {
  const HttpServer server(test_config("odr-http-server-test-unbound"));

  // cpp-httplib's listen_after_bind() reports success for this
  EXPECT_THROW(server.listen(), ServerNotBound);
}
