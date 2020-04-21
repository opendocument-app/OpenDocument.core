#include <access/FileUtil.h>
#include <access/Path.h>
#include <httplib.h>
#include <odr/Config.h>
#include <odr/Meta.h>
#include <odr/OpenDocumentReader.h>
#include <string>
#include <utility>

namespace {
struct Config {
  std::string tmpDir;
  std::string htmlDir;
};

class Server final {
public:
  explicit Server(Config config) : config(std::move(config)) {
    server.Get("/", [&](const auto &req, auto &res) {
      const char *html = R"V0G0N(
<!DOCTYPE html>
<html>
<head>
    <title>odr</title>
</head>
<body>
    <form action="/app/open" method="post" enctype="multipart/form-data">
        <input type="file" id="file" name="file">
        <input type="submit">
    </form>
</body>
</html>
  )V0G0N";

      res.set_content(html, "text/html");
    });

    server.Post("/app/open", [&](const auto &req, auto &res) {
      if (!req.has_file("file")) {
        res.set_content("no file", "text/plain");
        return;
      }

      const auto &file = req.get_file_value("file");
      const auto tmpInputFile = odr::access::Path{config.tmpDir}.join("input");
      const auto tmpOutputFile =
          odr::access::Path{config.tmpDir}.join("output");
      odr::access::FileUtil::write(file.content, tmpInputFile);

      bool success{true};

      success = odr.open(tmpInputFile);
      if (!success) {
        res.set_content("cannot open", "text/plain");
        return;
      }

      if (odr.getMeta().encrypted) {
        res.set_content("file encrypted", "text/plain");
        return;
      }

      ::odr::Config odrConfig{};
      success = odr.translate(tmpOutputFile, odrConfig);
      if (!success) {
        res.set_content("cannot translate", "text/plain");
        return;
      }

      const auto html = odr::access::FileUtil::read(tmpOutputFile);
      res.set_content(html, "text/html");
    });

    server.Post("/api/open", [&](const auto &req, auto &res) {});
  }

  void listen(const std::string &endpoint, const int port) {
    server.listen(endpoint.c_str(), port);
  }

private:
  const Config config;
  httplib::Server server;
  odr::OpenDocumentReader odr;
};
} // namespace

int main(int argc, char **argv) {
  const std::string endpoint{"0.0.0.0"};
  const int port{std::stoi(argv[1])};

  Config config;
  config.tmpDir = "/tmp";

  Server server{config};
  server.listen(endpoint, port);

  return 0;
}
