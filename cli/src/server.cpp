#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/http_server.hpp>

#include <iostream>
#include <string>

using namespace odr;

int main(int argc, char **argv) {
  std::string input{argv[1]};

  std::optional<std::string> password;
  if (argc >= 3) {
    password = argv[2];
  }

  DecodePreference decode_preference;
  decode_preference.engine_priority = {
      DecoderEngine::poppler, DecoderEngine::wvware, DecoderEngine::odr};

  DecodedFile decoded_file{input, decode_preference};

  if (decoded_file.is_document_file()) {
    DocumentFile document_file = decoded_file.document_file();
    if (document_file.password_encrypted() && !password) {
      std::cerr << "document encrypted but no password given" << std::endl;
      return 2;
    }
    if (document_file.password_encrypted() &&
        !document_file.decrypt(*password)) {
      std::cerr << "wrong password" << std::endl;
      return 1;
    }
  }

  HttpServer::Config server_config;
  HttpServer server(server_config);

  HtmlConfig html_config;
  html_config.embed_images = false;
  html_config.embed_shipped_resources = false;
  html_config.relative_resource_paths = false;
  html_config.text_document_margin = true;

  {
    std::string prefix = "one_file";
    HtmlViews views = server.serve_file(decoded_file, prefix, html_config);
    std::cout << "hosted decoded file with id: " << prefix << std::endl;
    for (const auto &view : views) {
      std::cout << "http://localhost:8080/file/" << prefix << "/" << view.path()
                << std::endl;
    }
  }

  server.listen("localhost", 8080);

  return 0;
}
