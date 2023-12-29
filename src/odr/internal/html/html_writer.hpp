#ifndef ODR_INTERNAL_HTML_HTML_WRITER_HPP
#define ODR_INTERNAL_HTML_HTML_WRITER_HPP

#include <odr/html.hpp>

#include <functional>
#include <iosfwd>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::html {

enum class HtmlCloseType {
  standard,
  trailing,
  none,
};

using HtmlWriteCallback = std::function<void(std::ostream &)>;
using HtmlWritable = std::variant<const char *, std::string, HtmlWriteCallback>;
using HtmlAttributesVector = std::vector<std::pair<HtmlWritable, HtmlWritable>>;
using HtmlAttributeWriterCallback =
    std::function<void(const HtmlWritable &, const HtmlWritable &)>;
using HtmlAttributeCallback =
    std::function<void(const HtmlAttributeWriterCallback &)>;
using HtmlAttributes =
    std::variant<HtmlAttributesVector, HtmlAttributeCallback>;

struct HtmlElementOptions {
  bool inline_element{false};
  HtmlCloseType close_type{HtmlCloseType::standard};

  std::optional<HtmlAttributes> attributes{};

  std::optional<HtmlWritable> style{};
  std::optional<HtmlWritable> clazz{};

  std::optional<HtmlWritable> extra{};

  HtmlElementOptions &set_inline(bool);
  HtmlElementOptions &set_close_type(HtmlCloseType);
  HtmlElementOptions &set_attributes(std::optional<HtmlAttributes>);
  HtmlElementOptions &set_style(std::optional<HtmlWritable>);
  HtmlElementOptions &set_class(std::optional<HtmlWritable>);
  HtmlElementOptions &set_extra(std::optional<HtmlWritable>);
};

class HtmlWriter {
public:
  explicit HtmlWriter(std::ostream &out, bool format, std::uint8_t indent);

  void write_begin();
  void write_end();

  void write_header_begin();
  void write_header_end();
  void write_header_title(const std::string &title);
  void write_header_viewport(const std::string &viewport);
  void write_header_target(const std::string &target);
  void write_header_charset(const std::string &charset);
  void write_header_style(const std::string &href);
  void write_header_style_begin();
  void write_header_style_end();

  void write_script(const std::string &src);
  void write_script_begin();
  void write_script_end();

  void write_body_begin(HtmlElementOptions options = {});
  void write_body_end();

  void write_element_begin(const std::string &name,
                           HtmlElementOptions options = {});
  void write_element_end(const std::string &name);

  bool is_inline_mode() const;
  void write_new_line();
  void write_raw(const HtmlWritable &writable, bool new_line = true);

  std::ostream &out();

private:
  struct StackElement {
    std::string name;
    bool inline_element{false};
  };

  std::ostream &m_out;
  bool m_format{false};
  std::string m_indent;
  std::uint32_t m_current_indent{0};
  std::vector<StackElement> m_stack;
};

} // namespace odr::internal::html

#endif // ODR_INTERNAL_HTML_HTML_WRITER_HPP
