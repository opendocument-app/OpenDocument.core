#include <common/HtmlWriter.h>

namespace odr {
namespace common {

HtmlDocumentWriter::HtmlDocumentWriter(std::unique_ptr<access::Sink> sink) : sink_(std::move(sink)) {
  addDoctype_();
}

HtmlDocumentWriter::~HtmlDocumentWriter() {
  sink_->write("</html>", 7);
}

void HtmlDocumentWriter::addDoctype_() {
  sink_->write("<!DOCTYPE html>\n", 16);
}

}
}
