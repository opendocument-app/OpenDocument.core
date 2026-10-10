#include <odr/exceptions.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>

#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <ostream>

using namespace odr;
namespace ihtml = odr::internal::html;

namespace {

class FailingResource final : public ihtml::HtmlResource {
public:
  FailingResource()
      : ihtml::HtmlResource(HtmlResourceType::image, "image/png", "failed.png",
                            "failed.png", std::nullopt, false, false, true) {}

  void write_resource(std::ostream &out) const override {
    out.setstate(std::ios::badbit);
  }
};

class FailingView final : public ihtml::HtmlView {
public:
  FailingView(const internal::abstract::HtmlService &service,
              const bool fail_resource)
      : ihtml::HtmlView(service, "failed", 0, "failed.html"),
        m_fail_resource{fail_resource} {}

  HtmlResources write_html(ihtml::HtmlWriter &out) const override {
    if (m_fail_resource) {
      out.out() << "<html></html>";
      return {
          {HtmlResource(std::make_shared<FailingResource>()), "failed.png"}};
    }
    out.out().setstate(std::ios::badbit);
    return {};
  }

private:
  const bool m_fail_resource;
};

} // namespace

TEST(HtmlExport, failed_pages_and_resources_are_reported) {
  const HtmlService service =
      html::translate(open(File::from_memory("hello", "hello.txt")), {});
  for (const bool fail_resource : {false, true}) {
    SCOPED_TRACE(fail_resource);
    const HtmlView view(
        std::make_shared<FailingView>(*service.impl(), fail_resource));
    EXPECT_THROW(static_cast<void>(service.bring_offline(
                     "output/html_export_failure", {view})),
                 FileWriteError);
    EXPECT_THROW(
        static_cast<void>(view.bring_offline("output/html_export_failure")),
        FileWriteError);
  }
}
