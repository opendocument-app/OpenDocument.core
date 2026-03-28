#pragma once

#include <odr/internal/oldms/word/structs.hpp>

#include <array>
#include <iosfwd>

namespace odr::internal::oldms {

void read(std::istream &in, Fib &fib);
void read(std::istream &in, FibBase &fibBase);
void read(std::istream &in, FibRgFcLcb97 &out);
void read(std::istream &in, FibRgFcLcb2000 &out);
void read(std::istream &in, FibRgFcLcb2002 &out);
void read(std::istream &in, FibRgFcLcb2003 &out);
void read(std::istream &in, FibRgFcLcb2007 &out);
void read(std::istream &in, FibRgCswNew &out);

std::unique_ptr<FibRgFcLcb97> readFibRgFcLcb(std::istream &in,
                                             std::uint16_t nFib);

} // namespace odr::internal::oldms
