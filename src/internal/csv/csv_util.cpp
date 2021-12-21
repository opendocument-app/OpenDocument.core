#include <csv.hpp>
#include <internal/csv/csv_util.h>
#include <internal/util/stream_util.h>

namespace odr::internal {

void csv::check_csv_file(std::istream &in) {
  // TODO limit check size
  ::csv::CSVReader parser;
  // TODO feed in junks
  parser.feed(util::stream::read(in));
  parser.end_feed();
  // TODO check if that even works; if not we have to write the CSV to disk
  // first
  // https://github.com/vincentlaucsb/csv-parser#memory-mapped-files-vs-streams
}

} // namespace odr::internal
