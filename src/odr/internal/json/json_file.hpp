#pragma once

#include <odr/file.hpp>

#include <odr/internal/text/text_file.hpp>

#include <memory>

namespace odr::internal::json {

class JsonFile final : public abstract::TextFile {
public:
  explicit JsonFile(std::shared_ptr<text::TextFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

private:
  std::shared_ptr<text::TextFile> m_file;
  std::string m_charset;
};

} // namespace odr::internal::json
