#include <odr/internal/oldms/text/doc_helper.hpp>

#include <odr/internal/oldms/text/doc_io.hpp>
#include <odr/internal/oldms/text/doc_style.hpp>
#include <odr/internal/util/byte_stream_util.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <array>
#include <cstring>
#include <istream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

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

void text::CharacterRuns::append_run(const std::uint32_t begin_fc,
                                     const std::uint32_t end_fc,
                                     const std::uint32_t style_index) {
  if (begin_fc >= end_fc ||
      (!m_runs.empty() && begin_fc < m_runs.back().end_fc)) {
    throw std::runtime_error("doc: character runs must be ascending");
  }
  if (!m_runs.empty() && m_runs.back().end_fc == begin_fc &&
      m_runs.back().style_index == style_index) {
    m_runs.back().end_fc = end_fc;
    return;
  }
  m_runs.push_back({begin_fc, end_fc, style_index});
}

std::uint32_t text::CharacterRuns::index_at(const std::uint32_t fc) const {
  const auto it = std::ranges::upper_bound(m_runs, fc, {}, &Run::begin_fc);
  if (it == m_runs.begin()) {
    return 0;
  }
  const Run &run = *std::prev(it);
  return fc < run.end_fc ? run.style_index : 0;
}

std::uint32_t text::CharacterRuns::chunk_end(const std::uint32_t fc) const {
  const auto it = std::ranges::upper_bound(m_runs, fc, {}, &Run::begin_fc);
  if (it != m_runs.begin() && fc < std::prev(it)->end_fc) {
    return std::prev(it)->end_fc;
  }
  if (it != m_runs.end()) {
    return it->begin_fc;
  }
  return std::numeric_limits<std::uint32_t>::max();
}

text::CharacterRuns text::read_character_runs(std::istream &document_stream,
                                              std::istream &table_stream,
                                              const FcLcb plcf_bte_chpx,
                                              StyleRegistry &style_registry) {
  CharacterRuns result;
  if (plcf_bte_chpx.lcb == 0) {
    return result;
  }

  // Copy: `add_style` may reallocate the registry's style storage.
  const TextStyle default_style = style_registry.text_style(0);

  table_stream.seekg(plcf_bte_chpx.fc);
  std::string plc_bytes = util::stream::read(table_stream, plcf_bte_chpx.lcb);
  const PlcBteChpxMap plc(plc_bytes.data(), plc_bytes.size());

  // One resolved style per distinct Chpx byte sequence.
  std::unordered_map<std::string, std::uint32_t> style_cache;
  const auto resolve = [&](const std::string_view grpprl) -> std::uint32_t {
    if (grpprl.empty()) {
      return 0;
    }
    const auto it = style_cache.find(std::string(grpprl));
    if (it != style_cache.end()) {
      return it->second;
    }
    const std::uint32_t index = style_registry.add_style(apply_character_sprms(
        default_style, grpprl, style_registry.font_names()));
    style_cache.emplace(std::string(grpprl), index);
    return index;
  };

  for (std::uint32_t i = 0; i < plc.n(); ++i) {
    // A 512-byte ChpxFkp page ([MS-DOC] 2.9.33) at pn * 512: crun in the last
    // byte, crun+1 rgfc offsets, crun rgb bytes; rgb[j] * 2 locates the Chpx
    // within the page, 0 means default properties.
    std::array<char, 512> page;
    document_stream.seekg(static_cast<std::streamoff>(plc.aData(i).pn) * 512);
    document_stream.read(page.data(), page.size());
    if (!document_stream) {
      throw std::runtime_error("doc: truncated ChpxFkp");
    }

    const auto crun = static_cast<std::uint8_t>(page[511]);
    if (crun < 0x01 || crun > 0x65) {
      throw std::runtime_error("doc: ChpxFkp crun out of range");
    }

    for (std::uint8_t j = 0; j < crun; ++j) {
      std::uint32_t begin_fc;
      std::uint32_t end_fc;
      std::memcpy(&begin_fc, page.data() + static_cast<std::size_t>(j) * 4, 4);
      std::memcpy(&end_fc, page.data() + static_cast<std::size_t>(j + 1) * 4,
                  4);

      const auto rgb = static_cast<std::uint8_t>(
          page[static_cast<std::size_t>(crun + 1) * 4 + std::size_t{j}]);
      std::string_view grpprl;
      if (rgb != 0) {
        const std::size_t chpx_at = static_cast<std::size_t>(rgb) * 2;
        if (chpx_at >= page.size() - 1) {
          throw std::runtime_error("doc: Chpx offset out of range");
        }
        const auto cb = static_cast<std::uint8_t>(page[chpx_at]);
        if (chpx_at + 1 + cb > page.size() - 1) {
          throw std::runtime_error("doc: Chpx overruns its ChpxFkp");
        }
        grpprl = std::string_view(page.data() + chpx_at + 1, cb);
      }

      result.append_run(begin_fc, end_fc, resolve(grpprl));
    }
  }

  return result;
}

} // namespace odr::internal::oldms
