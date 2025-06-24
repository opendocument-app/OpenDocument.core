#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/http_server.hpp>

#include <iostream>
#include <string>

using namespace odr;

int main(int argc, char **argv) {
  auto logger = Logger::create_stdio("odr-server", LogLevel::verbose);

  std::string input{argv[1]};

  std::optional<std::string> password;
  if (argc >= 3) {
    password = argv[2];
  }

  DecodePreference decode_preference;
  decode_preference.engine_priority = {
      DecoderEngine::poppler, DecoderEngine::wvware, DecoderEngine::odr};

  DecodedFile decoded_file{input, decode_preference, *logger};

  if (decoded_file.password_encrypted() && !password) {
    ODR_FATAL(*logger, "document encrypted but no password given");
    return 2;
  }
  if (decoded_file.password_encrypted()) {
    try {
      decoded_file = decoded_file.decrypt(*password);
    } catch (const WrongPasswordError &) {
      ODR_FATAL(*logger, "wrong password");
      return 1;
    }
  }

  HttpServer::Config server_config;
  HttpServer server(server_config);

  HtmlConfig html_config;
  html_config.embed_images = false;
  html_config.embed_shipped_resources = true;
  html_config.relative_resource_paths = false;
  html_config.text_document_margin = true;

  {
    std::string prefix = "one_file";
    HtmlViews views = server.serve_file(decoded_file, prefix, html_config);
    ODR_INFO(*logger, "hosted decoded file with id: " << prefix);
    for (const auto &view : views) {
      ODR_INFO(*logger,
               "http://localhost:8080/file/" << prefix << "/" << view.path());
    }
  }

  server.listen("localhost", 8080);

  return 0;
}
