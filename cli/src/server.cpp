#include <odr/file.hpp>
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

  HttpServer::Config config;
  HttpServer server(config);

  {
    std::string id = server.host_file(File(input));
    std::cout << "hosted file with id: " << id << std::endl;
    std::cout << "http://localhost:8080/" << id << std::endl;
  }

  {
    std::string id = server.host_file(decoded_file);
    std::cout << "hosted decoded file with id: " << id << std::endl;
    std::cout << "http://localhost:8080/" << id << std::endl;
  }

  server.listen("localhost", 8080);

  return 0;
}
