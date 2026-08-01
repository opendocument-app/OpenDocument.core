#include <odr/archive.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/http_server.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

using namespace odr;

int main(const int argc, char **argv) {
  try {
    const Logger logger = Logger::create_stdio("odr-server", LogLevel::verbose);

    std::string input{argv[1]};

    std::optional<std::string> password;
    if (argc >= 3) {
      password = argv[2];
    }

    DecodePreference decode_preference;
    decode_preference.as_file_type = FileType::zip;

    DecodedFile decoded_file{input, decode_preference, logger};

    if (decoded_file.password_encrypted()) {
      if (!password) {
        ODR_FATAL(logger, "document encrypted but no password given");
        return 2;
      }
      try {
        decoded_file = decoded_file.decrypt(*password);
      } catch (const WrongPasswordError &) {
        ODR_FATAL(logger, "wrong password");
        return 1;
      }
    }

    const HttpServer server{{}, logger};

    // the server does not own a cache any more, so the translation goes
    // somewhere of our choosing
    const std::filesystem::path cache_path =
        std::filesystem::temp_directory_path() / "odr-server";
    std::filesystem::remove_all(cache_path);

    // bind before anything is printed: the port is only known once the socket
    // is, and it is not necessarily the one that was asked for
    const std::uint32_t port = server.bind("localhost", 8080);
    const std::string base_url = "http://localhost:" + std::to_string(port);

    HtmlConfig html_config;
    html_config.embed_images = false;
    html_config.text_document_margin = true;
    html_config.editable = true;

    {
      const std::string prefix = "file";
      const std::string prefix_cache_path = (cache_path / prefix).string();
      std::filesystem::create_directories(prefix_cache_path);

      const HtmlService service =
          html::translate(decoded_file, prefix_cache_path, html_config, logger);
      server.connect_service(service, prefix);
      const HtmlViews views = service.list_views();
      ODR_INFO(logger, "hosted decoded file with id: " << prefix);
      for (const auto &view : views) {
        ODR_INFO(logger, base_url << "/file/" << prefix << "/" << view.path());
      }
    }

    if (decoded_file.is_document_file() || decoded_file.is_archive_file()) {
      const Filesystem filesystem =
          decoded_file.is_document_file()
              ? decoded_file.as_document_file().document().as_filesystem()
              : decoded_file.as_archive_file().archive().as_filesystem();

      const std::string prefix = "filesystem";
      const std::string prefix_cache_path = (cache_path / prefix).string();
      std::filesystem::create_directories(prefix_cache_path);

      const HtmlService filesystem_service =
          html::translate(filesystem, prefix_cache_path, html_config, logger);
      server.connect_service(filesystem_service, prefix);
      ODR_INFO(logger, "hosted filesystem with id: " << prefix);
      for (const auto &view : filesystem_service.list_views()) {
        ODR_INFO(logger, base_url << "/file/" << prefix << "/" << view.path());
      }
    }

    server.listen();

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
