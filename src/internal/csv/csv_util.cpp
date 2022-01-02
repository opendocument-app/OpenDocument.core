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

  // TODO feed in junks; limit check size
  auto parser = ::csv::parse(util::stream::read(in), format);

  // this will actually check `variable_columns`
  for (auto &&_ : parser) {
  }

  if (parser.get_col_names().size() <= 1) {
    throw std::runtime_error("no csv file");
  }
}

} // namespace odr::internal
