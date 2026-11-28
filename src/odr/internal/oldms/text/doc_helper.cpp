#include <odr/internal/oldms/text/doc_helper.hpp>

#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/util/byte_stream_util.hpp>

#include <array>
#include <cstring>
#include <istream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace odr::internal::oldms::text {

void FormattingRuns::append_run(const std::uint32_t begin_fc,
                                const std::uint32_t end_fc,
                                const std::uint32_t index) {
  if (begin_fc >= end_fc ||
      (!m_runs.empty() && begin_fc < m_runs.back().end_fc)) {
    throw std::runtime_error("doc: formatting runs must be ascending");
  }
  if (!m_runs.empty() && m_runs.back().end_fc == begin_fc &&
      m_runs.back().index == index) {
    m_runs.back().end_fc = end_fc;
    return;
  }
  m_runs.emplace_back(begin_fc, end_fc, index);
}

std::uint32_t FormattingRuns::index_at(const std::uint32_t fc) const {
  const auto it = std::ranges::upper_bound(m_runs, fc, {}, &Run::begin_fc);
  if (it == m_runs.begin()) {
    return 0;
  }
  const Run &run = *std::prev(it);
  return fc < run.end_fc ? run.index : 0;
}

std::uint32_t FormattingRuns::chunk_end(const std::uint32_t fc) const {
  const auto it = std::ranges::upper_bound(m_runs, fc, {}, &Run::begin_fc);
  if (it != m_runs.begin() && fc < std::prev(it)->end_fc) {
    return std::prev(it)->end_fc;
  }
  if (it != m_runs.end()) {
    return it->begin_fc;
  }
  return std::numeric_limits<std::uint32_t>::max();
}

} // namespace odr::internal::oldms::text

namespace odr::internal::oldms {

namespace {

using Fkp = std::array<char, 512>;

/// The runs of every FKP page a PlcBte* locates ([MS-DOC] 2.2.3): ascending
/// rgfc boundaries, then `entry_size` bytes per run whose first byte, times
/// two, locates its properties; 0 means none. The run count is the last byte.
/// `properties_at` returns a run's property bytes, which index `properties`
/// by their first occurrence.
template <typename PlcBte, typename PropertiesAt>
text::FormattingRuns
read_fkp_runs(std::istream &document_stream, std::istream &table_stream,
              const text::FcLcb plcf_bte, const std::size_t entry_size,
              const std::uint8_t max_runs, std::vector<std::string> &properties,
              const PropertiesAt &properties_at) {
  text::FormattingRuns result;
  if (plcf_bte.lcb == 0) {
    return result;
  }

  table_stream.seekg(plcf_bte.fc);
  const std::string plc_bytes =
      util::byte_stream::read_u8s(table_stream, plcf_bte.lcb);
  const PlcBte plc(plc_bytes.data(), plc_bytes.size());

  std::unordered_map<std::string, std::uint32_t> indices;
  for (std::uint32_t i = 0; i < properties.size(); ++i) {
    indices.try_emplace(properties[i], i);
  }
  const auto index_of = [&](const std::string_view bytes) -> std::uint32_t {
    if (!std::in_range<std::uint32_t>(properties.size())) {
      throw std::length_error("doc: too many distinct properties");
    }
    const auto [it, inserted] = indices.try_emplace(
        std::string(bytes), static_cast<std::uint32_t>(properties.size()));
    if (inserted) {
      properties.emplace_back(bytes);
    }
    return it->second;
  };

  const std::size_t page_count = plc.n();
  for (std::size_t i = 0; i < page_count; ++i) {
    Fkp page;
    document_stream.seekg(static_cast<std::streamoff>(plc.aData(i).pn) * 512);
    document_stream.read(page.data(), page.size());
    if (!document_stream) {
      throw std::runtime_error("doc: truncated Fkp");
    }

    const auto count = static_cast<std::uint8_t>(page.back());
    if (count < 1 || count > max_runs) {
      throw std::runtime_error("doc: Fkp run count out of range");
    }

    for (std::uint8_t j = 0; j < count; ++j) {
      std::uint32_t begin_fc;
      std::uint32_t end_fc;
      std::memcpy(&begin_fc, page.data() + static_cast<std::size_t>(j) * 4, 4);
      std::memcpy(&end_fc, page.data() + static_cast<std::size_t>(j + 1) * 4,
                  4);

      const auto offset = static_cast<std::uint8_t>(
          page[static_cast<std::size_t>(count + 1) * 4 + j * entry_size]);
      std::uint32_t index = 0;
      if (offset != 0) {
        if (static_cast<std::size_t>(offset) * 2 >= page.size() - 1) {
          throw std::runtime_error("doc: Fkp property offset out of range");
        }
        index = index_of(properties_at(page, offset));
      }
      result.append_run(begin_fc, end_fc, index);
    }
  }

  return result;
}

} // namespace

text::CharacterIndex text::read_character_index(std::istream &in) {
  CharacterIndex result;

  read_Clx(in, skip_Prc, [&](std::istream &) {
    if (const int c = in.get(); c != 0x2) {
      throw std::runtime_error("Unexpected input: " + std::to_string(c));
    }
    const std::uint32_t lcb = util::byte_stream::read<std::uint32_t>(in);
    std::string plcPcd = util::byte_stream::read_u8s(in, lcb);
    const PlcPcdMap plc_pcd_map(plcPcd.data(), plcPcd.size());

    const std::size_t count = plc_pcd_map.n();
    for (std::size_t i = 0; i < count; ++i) {
      // aCp is strictly ascending ([MS-DOC] 2.8.35); otherwise the piece
      // length below would wrap around.
      if (plc_pcd_map.aCP(i + 1) <= plc_pcd_map.aCP(i)) {
        throw std::runtime_error("doc: PlcPcd CPs must be ascending");
      }
      const FcCompressed fc = plc_pcd_map.aData(i).fc;
      const bool is_compressed = fc.fCompressed != 0;
      const std::size_t data_offset = is_compressed ? fc.fc / 2 : fc.fc;
      const std::size_t length_cp = plc_pcd_map.aCP(i + 1) - plc_pcd_map.aCP(i);
      result.append(plc_pcd_map.aCP(i), length_cp, data_offset, is_compressed);
    }
  });

  return result;
}

text::FormattingRuns
text::read_character_runs(std::istream &document_stream,
                          std::istream &table_stream, const FcLcb plcf_bte_chpx,
                          std::vector<std::string> &grpprls) {
  // A ChpxFkp ([MS-DOC] 2.9.33) holds 1 to 0x65 runs, a one-byte rgb each;
  // the Chpx at rgb * 2 is cb and a grpprl of cb bytes.
  return read_fkp_runs<PlcBteChpxMap>(
      document_stream, table_stream, plcf_bte_chpx, 1, 0x65, grpprls,
      [](const Fkp &page, const std::uint8_t offset) {
        const std::size_t at = static_cast<std::size_t>(offset) * 2;
        const auto cb = static_cast<std::uint8_t>(page[at]);
        if (at + 1 + cb > page.size() - 1) {
          throw std::runtime_error("doc: Chpx overruns its ChpxFkp");
        }
        return std::string_view(page.data() + at + 1, cb);
      });
}

text::FormattingRuns
text::read_paragraph_runs(std::istream &document_stream,
                          std::istream &table_stream, const FcLcb plcf_bte_papx,
                          std::vector<std::string> &papxs) {
  // A PapxFkp ([MS-DOC] 2.9.209) holds 1 to 0x1D paragraphs, a 13-byte BxPap
  // each whose first byte locates the PapxInFkp ([MS-DOC] 2.9.210): a cb of 0
  // is followed by the size in words, otherwise the size is 2 * cb - 1.
  return read_fkp_runs<PlcBtePapxMap>(
      document_stream, table_stream, plcf_bte_papx, 13, 0x1D, papxs,
      [](const Fkp &page, const std::uint8_t offset) {
        std::size_t at = static_cast<std::size_t>(offset) * 2;
        std::size_t size =
            2 * static_cast<std::size_t>(static_cast<std::uint8_t>(page[at]));
        ++at;
        if (size == 0) {
          size =
              2 * static_cast<std::size_t>(static_cast<std::uint8_t>(page[at]));
          ++at;
        } else {
          --size;
        }
        if (size < 2 || at + size > page.size() - 1) {
          throw std::runtime_error("doc: Papx overruns its PapxFkp");
        }
        return std::string_view(page.data() + at, size);
      });
}

} // namespace odr::internal::oldms
