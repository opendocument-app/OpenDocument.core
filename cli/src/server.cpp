#include <odr/Server.h>
#include <string>

int main(int argc, char **argv) {
  const std::string endpoint{"0.0.0.0"};
  const int port{std::stoi(argv[1])};

  odr::ServerConfig config;
  config.tmpDir = "/tmp";

  odr::Server server{config};
  server.listen(endpoint, port);

  return 0;
}
