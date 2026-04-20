#pragma once

#include <odr/internal/oldms/text/doc_structs.hpp>

#include <array>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>

namespace odr::internal::oldms::text {

void read(std::istream &in, FibBase &out);
void read(std::istream &in, FibRgFcLcb97 &out);
void read(std::istream &in, FibRgFcLcb2000 &out);
void read(std::istream &in, FibRgFcLcb2002 &out);
void read(std::istream &in, FibRgFcLcb2003 &out);
void read(std::istream &in, FibRgFcLcb2007 &out);

std::size_t determine_size_Fib(std::istream &in);

std::unique_ptr<FibRgFcLcb97> read_FibRgFcLcb(std::istream &in,
                                              std::uint16_t nFib);
void read(std::istream &in, ParsedFibRgCswNew &out);
void read(std::istream &in, ParsedFib &out);

using HandlePrc = std::function<void(std::istream &in)>;
using HandlePcdt = std::function<void(std::istream &in)>;

void read_Clx(std::istream &in, const HandlePrc &handle_Prc,
              const HandlePcdt &handle_Pcdt);
void skip_Prc(std::istream &in);

std::string read_string(std::istream &in, std::size_t length_cp,
                        bool is_compressed);
std::string read_string_compressed(std::istream &in, std::size_t length_cp);
std::u16string read_string_uncompressed(std::istream &in,
                                        std::size_t length_cp);

std::optional<char16_t> uncompress_char(char c);

} // namespace odr::internal::oldms::text
