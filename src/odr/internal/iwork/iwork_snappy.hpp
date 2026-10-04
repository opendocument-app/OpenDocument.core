#pragma once

#include <string>
#include <string_view>

namespace odr::internal::iwork {

/// Decompresses one Snappy block, without Snappy stream framing or checksums.
std::string snappy_decompress_block(std::string_view compressed);

/// Decodes repeated `.iwa` blocks: zero, 24-bit length, Snappy payload.
/// Verified on `empty.pages Index/Document.iwa +0`.
std::string iwa_decompress(std::string_view framed);

} // namespace odr::internal::iwork
