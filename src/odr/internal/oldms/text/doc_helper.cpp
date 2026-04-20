#include <odr/internal/oldms/text/doc_helper.hpp>

#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <istream>

namespace odr::internal::oldms {

text::CharacterIndex text::read_character_index(std::istream &in) {
  CharacterIndex result;

  read_Clx(in, skip_Prc, [&](std::istream &) {
    if (const int c = in.get(); c != 0x2) {
      throw std::runtime_error("Unexpected input: " + std::to_string(c));
    }
    const std::uint32_t lcb = util::byte_stream::read<std::uint32_t>(in);
    std::string plcPcd = util::stream::read(in, lcb);
    const PlcPcdMap plc_pcd_map(plcPcd.data(), plcPcd.size());

    for (std::uint32_t i = 0; i < plc_pcd_map.n(); ++i) {
      const bool is_compressed = plc_pcd_map.aData(i).fc.fCompressed != 0;
      const std::size_t data_offset = is_compressed
                                          ? plc_pcd_map.aData(i).fc.fc / 2
                                          : plc_pcd_map.aData(i).fc.fc;
      const std::size_t length_cp = plc_pcd_map.aCP(i + 1) - plc_pcd_map.aCP(i);
      result.append(plc_pcd_map.aCP(i), length_cp, data_offset, is_compressed);
    }
  });

  return result;
}

} // namespace odr::internal::oldms
