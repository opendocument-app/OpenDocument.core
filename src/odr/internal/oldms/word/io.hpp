#pragma once

#include <odr/internal/oldms/word/structs.hpp>

#include <array>
#include <functional>
#include <iosfwd>
#include <memory>

namespace odr::internal::oldms {

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

} // namespace odr::internal::oldms
