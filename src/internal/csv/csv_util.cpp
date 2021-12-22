#include <csv.hpp>
#include <internal/csv/csv_util.h>
#include <internal/util/stream_util.h>

namespace odr::internal {

void csv::check_csv_file(std::istream &in) {
  // TODO it might be better to read the file from disk to decide on the format
  // https://github.com/vincentlaucsb/csv-parser#memory-mapped-files-vs-streams

  ::csv::CSVFormat format;
  // TODO safe to say a CSV with variable columns is invalid?
  format.variable_columns(::csv::VariableColumnPolicy::THROW);

  ::csv::CSVReader parser(format);
  // TODO feed in junks; limit check size
  parser.feed(util::stream::read(in));
  parser.end_feed();

  // this will actually check `variable_columns`
  for (auto &&_ : parser) {
  }
}

} // namespace odr::internal
