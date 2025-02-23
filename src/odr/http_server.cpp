#include <odr/http_server.hpp>

#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/html_service.hpp>
#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/html/document.hpp>
#include <odr/internal/html/pdf2htmlex_wrapper.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <random>

#include <httplib/httplib.h>

namespace odr {

class HttpServer::Impl {
public:
  explicit Impl(const HttpServer::Config &config) : m_config{config} {
    m_server.Get("/",
                 [](const httplib::Request & /*req*/, httplib::Response &res) {
                   res.set_content("Hello World!", "text/plain");
                 });

    m_server.Get("/file/:id", [this](const httplib::Request &req,
                                     httplib::Response &res) {
      std::string id = req.path_params.at("id");

      std::unique_lock lock{m_mutex};
      auto it = m_content.find(id);
      if (it == m_content.end()) {
        res.status = 404;
        return;
      }
      lock.unlock();
      const Content &content = it->second;

      if (std::holds_alternative<File>(content.file)) {
        serve_file(res, std::get<File>(content.file));
      } else {
        serve_file(std::get<DecodedFile>(content.file), id, content.config);
      }
    });
  }

  void serve_file(httplib::Response &res, const File &file) {
    struct State {
      State(std::unique_ptr<std::istream> stream_, std::size_t buffer_size)
          : stream{std::move(stream_)}, buffer(buffer_size, 0) {}

      std::unique_ptr<std::istream> stream;
      std::vector<char> buffer;
    };

    auto state = std::make_shared<State>(file.stream(), m_config.buffer_size);
    httplib::ContentProvider content_provider =
        [state = std::move(state)](std::size_t offset, std::size_t length,
                                   httplib::DataSink &sink) -> bool {
      std::istream &stream = *state->stream;

      stream.seekg(static_cast<std::streamsize>(offset), std::ios::beg);
      stream.read(state->buffer.data(), static_cast<std::streamsize>(length));
      sink.write(state->buffer.data(),
                 static_cast<std::size_t>(stream.gcount()));

      return !stream.eof();
    };

    res.set_content_provider(file.size(), "application/octet-stream",
                             content_provider);
  }

  void serve_file(const httplib::Request &req, httplib::Response &res,
                  DecodedFile file, const std::string &prefix,
                  const HtmlConfig &config) {
    std::string output_path = m_config.output_path + "/" + prefix;

    std::filesystem::create_directories(output_path);

    HtmlService html_service;

    if (file.is_document_file()) {
      DocumentFile document_file = file.document_file();
      if (document_file.password_encrypted()) {
        throw std::runtime_error("Document is encrypted");
      }
      auto document = document_file.document();
      html_service = internal::html::create_document_service(
          document, output_path, config);
#ifdef ODR_WITH_PDF2HTMLEX
    } else if (file.is_pdf_file()) {
      PdfFile pdf_file = file.pdf_file();
      if (pdf_file.password_encrypted()) {
        throw std::runtime_error("Document is encrypted");
      }
      html_service = internal::html::create_poppler_pdf_service(
          dynamic_cast<const internal::PopplerPdfFile &>(*pdf_file.impl()),
          output_path, config);
#endif
    } else {
      throw std::runtime_error("Unsupported file type");
    }

    internal::common::NullStream null;
    HtmlResources resources = html_service.write_document(null);

    m_server.Get("/" + id, [id](const httplib::Request & /*req*/,
                                httplib::Response &res) {
      res.set_redirect("/" + id + "/document.html");
    });

    m_server.Get("/" + id + "/document.html",
                 [=](const httplib::Request & /*req*/, httplib::Response &res) {
                   httplib::ContentProviderWithoutLength content_provider =
                       [html_service](std::size_t offset,
                                      httplib::DataSink &sink) -> bool {
                     if (offset != 0) {
                       throw std::runtime_error(
                           "Invalid offset: " + std::to_string(offset) +
                           ". Must be 0.");
                     }
                     html_service.write_document(sink.os);
                     return false;
                   };
                   res.set_content_provider("text/html", content_provider);
                 });

    for (const auto &[resource, location] : resources) {
      if (!location.has_value() || resource.is_external()) {
        continue;
      }

      m_server.Get(
          "/" + id + "/" + location.value(),
          [=](const httplib::Request & /*req*/, httplib::Response &res) {
            httplib::ContentProviderWithoutLength content_provider =
                [resource](std::size_t offset,
                           httplib::DataSink &sink) -> bool {
              if (offset != 0) {
                throw std::runtime_error(
                    "Invalid offset: " + std::to_string(offset) +
                    ". Must be 0.");
              }
              resource.write_resource(sink.os);
              return false;
            };
            res.set_content_provider(resource.mime_type(), content_provider);
          });
    }
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
    HtmlConfig config;
    std::variant<File, DecodedFile> file;
  };

  std::mutex m_mutex;
  std::unordered_map<std::string, Content> m_content;
};

HttpServer::HttpServer(const Config &config)
    : m_impl{std::make_unique<Impl>(config)} {}

void HttpServer::host_file(File file, const std::string &prefix) {
  m_impl->host_file(std::move(file), prefix);
}

void HttpServer::host_file(DecodedFile file, const std::string &prefix,
                           const HtmlConfig &config) {
  m_impl->host_file(std::move(file), prefix, config);
}

void HttpServer::listen(const std::string &host, std::uint32_t port) {
  m_impl->listen(host, port);
}

void HttpServer::stop() { m_impl->stop(); }

} // namespace odr
