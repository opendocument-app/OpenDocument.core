#pragma once

#include <string>
#include <string_view>

namespace odr::internal::iwork {

/// Decompresses one Snappy block — a varint uncompressed length followed by
/// literal and copy tags. The stream framing Snappy ships with (the `sNaPpY`
/// identifier, per-chunk CRC-32C) is not involved, see @ref iwa_decompress.
std::string snappy_decompress_block(std::string_view compressed);

/// Undoes the framing of an `.iwa`: `0x00`, a little-endian 24-bit compressed
/// length, then that many bytes of a Snappy block, repeated to the end.
/// Verified on `empty.pages Index/Document.iwa +0`.
std::string iwa_decompress(std::string_view framed);

} // namespace odr::internal::iwork
