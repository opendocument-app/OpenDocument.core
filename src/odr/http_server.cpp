#include <odr/http_server.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/html_service.hpp>
#include <odr/internal/html/document.hpp>
#include <odr/internal/html/pdf2htmlex_wrapper.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <httplib/httplib.h>

namespace odr {

class HttpServer::Impl {
public:
  explicit Impl(HttpServer::Config config) : m_config{std::move(config)} {
    m_server.Get("/",
                 [](const httplib::Request & /*req*/, httplib::Response &res) {
                   res.set_content("Hello World!", "text/plain");
                 });

    m_server.Get("/file/" + std::string(prefix_pattern),
                 [&](const httplib::Request &req, httplib::Response &res) {
                   serve_file(req, res);
                 });
    m_server.Get("/file/" + std::string(prefix_pattern) + "/(.*)",
                 [&](const httplib::Request &req, httplib::Response &res) {
                   serve_file(req, res);
                 });
  }

  const HttpServer::Config &config() const { return m_config; }

  void serve_file(const httplib::Request &req, httplib::Response &res) {
    std::string id = req.matches[1].str();
    std::string path = req.matches.size() > 1 ? req.matches[2].str() : "";

    std::unique_lock lock{m_mutex};
    auto it = m_content.find(id);
    if (it == m_content.end()) {
      res.status = 404;
      return;
    }
    const Content &content = it->second;

    serve_file(res, content.service, path);
  }

  void serve_file(httplib::Response &res, const HtmlService &service,
                  const std::string &path) {
    if (!service.exists(path)) {
      res.status = 404;
      return;
    }

    httplib::ContentProviderWithoutLength content_provider =
        [service, path](std::size_t offset, httplib::DataSink &sink) -> bool {
      if (offset != 0) {
        throw std::runtime_error("Invalid offset: " + std::to_string(offset) +
                                 ". Must be 0.");
      }
      service.write(path, sink.os);
      return false;
    };
    res.set_content_provider(service.mimetype(path), content_provider);
  }

  void connect_service(HtmlService service, const std::string &prefix) {
    m_content.emplace(prefix, Content{prefix, std::move(service)});
  }

  void listen(const std::string &host, std::uint32_t port) {
    m_server.listen(host, static_cast<int>(port));
  }

  void stop() { m_server.stop(); }

private:
  HttpServer::Config m_config;

  httplib::Server m_server;

  struct Content {
    std::string id;
    HtmlService service;
  };

  std::mutex m_mutex;
  std::unordered_map<std::string, Content> m_content;
};

HttpServer::HttpServer(const Config &config)
    : m_impl{std::make_unique<Impl>(config)} {}

void HttpServer::connect_service(HtmlService service,
                                 const std::string &prefix) {
  m_impl->connect_service(std::move(service), prefix);
}

void HttpServer::serve_file(DecodedFile file, const std::string &prefix,
                            const HtmlConfig &config) {
  static std::regex prefix_regex(prefix_pattern);
  if (!std::regex_match(prefix, prefix_regex)) {
    throw InvalidPrefix(prefix);
  }

  std::string output_path = m_impl->config().output_path + "/" + prefix;
  std::filesystem::create_directories(output_path);

  HtmlService service;

  if (file.is_document_file()) {
    service = internal::html::create_document_service(
        file.document_file().document(), output_path, config);
#ifdef ODR_WITH_PDF2HTMLEX
  } else if (file.is_pdf_file()) {
    PdfFile pdf_file = file.pdf_file();
    if (pdf_file.password_encrypted()) {
      throw std::runtime_error("Document is encrypted");
    }
    service = internal::html::create_poppler_pdf_service(
        dynamic_cast<const internal::PopplerPdfFile &>(*pdf_file.impl()),
        output_path, config);
#endif
  } else {
    throw std::runtime_error("Unsupported file type.");
  }

  m_impl->connect_service(std::move(service), prefix);
}

void HttpServer::listen(const std::string &host, std::uint32_t port) {
  m_impl->listen(host, port);
}

void HttpServer::stop() { m_impl->stop(); }

} // namespace odr
