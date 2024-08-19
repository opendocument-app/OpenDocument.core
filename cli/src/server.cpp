#include <odr/file.hpp>
#include <odr/html.hpp>

#include <iostream>
#include <string>

#include <httplib/httplib.h>

using namespace odr;

int main(int argc, char **argv) {
  std::string input{argv[1]};

  std::optional<std::string> password;
  if (argc >= 4) {
    password = argv[3];
  }

  DecodedFile decoded_file{input};

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

  HtmlConfig config;
  config.editable = true;

  httplib::Server svr;

  svr.Get("/hi", [](const httplib::Request &req, httplib::Response &res) {
    res.set_content("Hello World!", "text/plain");
  });

  // Match the request path against a regular expression
  // and extract its captures
  svr.Get(R"(/numbers/(\d+))",
          [&](const httplib::Request &req, httplib::Response &res) {
            auto numbers = req.matches[1];
            res.set_content(numbers, "text/plain");
          });

  // Capture the second segment of the request path as "id" path param
  svr.Get("/users/:id",
          [&](const httplib::Request &req, httplib::Response &res) {
            auto user_id = req.path_params.at("id");
            res.set_content(user_id, "text/plain");
          });

  // Extract values from HTTP headers and URL query params
  svr.Get("/body-header-param",
          [](const httplib::Request &req, httplib::Response &res) {
            if (req.has_header("Content-Length")) {
              auto val = req.get_header_value("Content-Length");
            }
            if (req.has_param("key")) {
              auto val = req.get_param_value("key");
            }
            res.set_content(req.body, "text/plain");
          });

  svr.Get("/stop", [&](const httplib::Request &req, httplib::Response &res) {
    svr.stop();
  });

  svr.listen("localhost", 12345);

  return 0;
}
