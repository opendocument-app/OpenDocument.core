#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace odr::internal::csv {

/// Reads the next record into @p fields; `false` once the input is exhausted.
///
/// RFC 4180: `,` separates, `"` quotes, `""` is a literal quote inside a quoted
/// field, and a quoted field may span lines. An empty line yields no record,
/// which is what keeps a trailing newline from reading as a one-field row.
///
/// @throws std::runtime_error if the input ends inside a quoted field.
bool read_record(std::istream &in, std::vector<std::string> &fields);

/// Throws unless @p in parses as a csv whose records all carry the same number
/// of fields, and more than one of them.
void check_csv_file(std::istream &in);

} // namespace odr::internal::csv
