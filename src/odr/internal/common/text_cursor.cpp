#include <odr/internal/common/text_cursor.hpp>

#include <stdexcept>

namespace odr::internal {

void TextCursor::throw_past_end() {
  throw std::out_of_range("TextCursor::advance: past end of input");
}

} // namespace odr::internal
