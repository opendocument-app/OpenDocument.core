#include <odr/http_server.hpp>

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>
#include <odr/html_service.hpp>
#include <odr/internal/html/document.hpp>
#include <odr/internal/html/pdf2htmlex_wrapper.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>
#include <odr/internal/util/file_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <random>

#include <httplib/httplib.h>

namespace odr {

class HttpServer::Impl {
public:
  explicit Impl(const HttpServer::Config &config) : m_config{config} {}

  [[nodiscard]] std::string host_file(File file) {
    std::string id = get_id();
    m_server.Get(
        "/" + id,
        [this, file = std::move(file)](const httplib::Request &req,
                                       httplib::Response &res) -> void {
          struct State {
            State(std::unique_ptr<std::istream> stream_,
                  std::size_t buffer_size)
                : stream{std::move(stream_)}, buffer(buffer_size, 0) {}

            std::unique_ptr<std::istream> stream;
            std::vector<char> buffer;
          };

          auto state =
              std::make_shared<State>(file.stream(), m_config.buffer_size);
          httplib::ContentProvider content_provider =
              [state = std::move(state)](std::size_t offset, std::size_t length,
                                         httplib::DataSink &sink) -> bool {
            std::istream &stream = *state->stream;

            stream.seekg(static_cast<std::streamsize>(offset), std::ios::beg);
            stream.read(state->buffer.data(),
                        static_cast<std::streamsize>(length));
            sink.write(state->buffer.data(),
                       static_cast<std::size_t>(stream.gcount()));

            return !stream.eof();
          };

          res.set_content_provider(file.size(), "application/octet-stream",
                                   content_provider);
        });
    return id;
  }

  [[nodiscard]] std::string host_file(DecodedFile file) {
    std::string id = get_id();

    // TODO
    HtmlConfig config;
    config.embed_resources = false;
    std::string output_path = "/tmp/" + id;
    std::string cache_path = output_path + "/cache";

    std::filesystem::create_directories(output_path);
    std::filesystem::create_directories(cache_path);

    HtmlDocumentService html_service;

    if (file.is_document_file()) {
      DocumentFile document_file = file.document_file();
      if (document_file.password_encrypted()) {
        throw std::runtime_error("Document is encrypted");
      }
      auto document = document_file.document();
      html_service = internal::html::create_document_service(
          document, output_path, config);
    } else if (file.is_pdf_file()) {
      PdfFile pdf_file = file.pdf_file();
      if (pdf_file.password_encrypted()) {
        throw std::runtime_error("Document is encrypted");
      }
      html_service = internal::html::create_poppler_pdf_service(
          dynamic_cast<const internal::PopplerPdfFile &>(*pdf_file.impl()),
          output_path, config);
    } else {
      throw std::runtime_error("Unsupported file type");
    }

    std::string document_html_path = cache_path + "/document.html";
    HtmlResources resources;
    {
      std::ofstream ostream(document_html_path);
      resources = html_service.write_document(ostream);
    }
    std::size_t document_html_size =
        internal::util::file::size(document_html_path);

    m_server.Get("/" + id,
                 [id](const httplib::Request &req, httplib::Response &res) {
                   res.set_redirect("/" + id + "/document.html");
                 });

    m_server.Get("/" + id + "/document.html", [=](const httplib::Request &req,
                                                  httplib::Response &res) {
      httplib::ContentProvider content_provider =
          [document_html_path,
           document_html_size](std::size_t offset, std::size_t length,
                               httplib::DataSink &sink) -> bool {
        if (offset != 0) {
          throw std::runtime_error("Invalid offset: " + std::to_string(offset) +
                                   ". Must be 0.");
        }
        if (length != document_html_size) {
          throw std::runtime_error("Invalid length: " + std::to_string(length) +
                                   ". Must be " +
                                   std::to_string(document_html_size) + ".");
        }
        std::ifstream in(document_html_path);
        internal::util::stream::pipe(in, sink.os);
        return false;
      };
      res.set_content_provider(document_html_size, "text/html",
                               content_provider);
    });

    for (const auto &[resource, location] : resources) {
      if (!location.has_value() || resource.is_external()) {
        continue;
      }

      m_server.Get("/" + id + "/" + location.value(),
                   [=](const httplib::Request &req, httplib::Response &res) {
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
                     res.set_content_provider(resource.mime_type(),
                                              content_provider);
                   });
    }

    return id;
  }

  [[nodiscard]] std::string host_filesystem(Filesystem filesystem) {
    std::string id = get_id();
    m_server.Get("/" + id,
                 [filesystem = std::move(filesystem)](
                     const httplib::Request &req, httplib::Response &res) {
                   res.set_content("Hello World!", "text/plain");
                 });
    return id;
  }

  void listen(const std::string &host, std::uint32_t port) {
    m_server.listen(host, static_cast<int>(port));
  }

private:
  [[nodiscard]] static std::string get_id() {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 9);

    std::string id(10, '0');
    for (char &c : id) {
      c = static_cast<char>('0' + dist(rng));
    }

    return id;
  }

  HttpServer::Config m_config;

  httplib::Server m_server;
};

HttpServer::HttpServer(const Config &config)
    : m_impl{std::make_unique<Impl>(config)} {}

std::string HttpServer::host_file(File file) {
  return m_impl->host_file(std::move(file));
}

std::string HttpServer::host_file(DecodedFile file) {
  return m_impl->host_file(std::move(file));
}

std::string HttpServer::host_filesystem(Filesystem filesystem) {
  return m_impl->host_filesystem(std::move(filesystem));
}

void HttpServer::listen(const std::string &host, std::uint32_t port) {
  m_impl->listen(host, port);
}

} // namespace odr
