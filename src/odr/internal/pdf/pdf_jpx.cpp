#include <odr/internal/pdf/pdf_jpx.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

#include <openjpeg.h>

namespace odr::internal {

namespace {

/// Cursor over the payload for openjpeg's stream callbacks.
struct MemoryStream {
  const std::string *data{nullptr};
  std::size_t position{0};
};

OPJ_SIZE_T stream_read(void *buffer, const OPJ_SIZE_T size, void *user) {
  auto *stream = static_cast<MemoryStream *>(user);
  const std::size_t left = stream->data->size() - stream->position;
  if (left == 0) {
    return static_cast<OPJ_SIZE_T>(-1);
  }
  const std::size_t n = std::min<std::size_t>(size, left);
  std::memcpy(buffer, stream->data->data() + stream->position, n);
  stream->position += n;
  return n;
}

OPJ_OFF_T stream_skip(const OPJ_OFF_T size, void *user) {
  auto *stream = static_cast<MemoryStream *>(user);
  const std::size_t left = stream->data->size() - stream->position;
  const auto n = static_cast<std::size_t>(std::max<OPJ_OFF_T>(size, 0));
  if (n > left) {
    stream->position = stream->data->size();
    return static_cast<OPJ_OFF_T>(-1);
  }
  stream->position += n;
  return static_cast<OPJ_OFF_T>(n);
}

OPJ_BOOL stream_seek(const OPJ_OFF_T position, void *user) {
  auto *stream = static_cast<MemoryStream *>(user);
  if (position < 0 ||
      static_cast<std::size_t>(position) > stream->data->size()) {
    return OPJ_FALSE;
  }
  stream->position = static_cast<std::size_t>(position);
  return OPJ_TRUE;
}

/// JP2 signature box (ISO/IEC 15444-1 I.5.1) vs a bare codestream's SOC + SIZ.
OPJ_CODEC_FORMAT codec_format(const std::string &data) {
  static constexpr std::array<unsigned char, 12> jp2_signature{
      0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20, 0x0d, 0x0a, 0x87, 0x0a};
  if (data.size() >= jp2_signature.size() &&
      std::memcmp(data.data(), jp2_signature.data(), jp2_signature.size()) ==
          0) {
    return OPJ_CODEC_JP2;
  }
  return OPJ_CODEC_J2K;
}

/// One component's sample at (x, y) of the *image* grid, scaled to 8 bits.
/// A component may be subsampled (`dx`/`dy`), which the nearest sample covers.
std::uint8_t sample_at(const opj_image_comp_t &comp, const std::int32_t x,
                       const std::int32_t y) {
  const std::uint32_t cx =
      std::min(static_cast<std::uint32_t>(x) / std::max(comp.dx, 1u),
               comp.w == 0 ? 0 : comp.w - 1);
  const std::uint32_t cy =
      std::min(static_cast<std::uint32_t>(y) / std::max(comp.dy, 1u),
               comp.h == 0 ? 0 : comp.h - 1);
  std::int32_t value = comp.data[cy * comp.w + cx];
  if (comp.sgnd != 0) {
    value += 1 << (comp.prec - 1);
  }
  const std::int32_t max = (1 << comp.prec) - 1;
  value = std::clamp(value, 0, max);
  return static_cast<std::uint8_t>(comp.prec >= 8 ? value >> (comp.prec - 8)
                                                  : value << (8 - comp.prec));
}

/// YCbCr -> RGB in place (ITU-R BT.601, the `sYCC` space JP2 may declare).
void sycc_to_rgb(std::string &samples) {
  for (std::size_t i = 0; i + 2 < samples.size(); i += 3) {
    const auto y = static_cast<double>(static_cast<std::uint8_t>(samples[i]));
    const double cb =
        static_cast<double>(static_cast<std::uint8_t>(samples[i + 1])) - 128.0;
    const double cr =
        static_cast<double>(static_cast<std::uint8_t>(samples[i + 2])) - 128.0;
    const auto to_byte = [](const double v) {
      return static_cast<char>(
          static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0)));
    };
    samples[i] = to_byte(y + 1.402 * cr);
    samples[i + 1] = to_byte(y - 0.344136 * cb - 0.714136 * cr);
    samples[i + 2] = to_byte(y + 1.772 * cb);
  }
}

} // namespace

std::optional<pdf::JpxImage> pdf::decode_jpx(const std::string &data) {
  if (data.empty()) {
    return std::nullopt;
  }

  const std::unique_ptr<opj_codec_t, decltype(&opj_destroy_codec)> codec(
      opj_create_decompress(codec_format(data)), &opj_destroy_codec);
  if (codec == nullptr) {
    return std::nullopt;
  }
  opj_dparameters_t parameters;
  opj_set_default_decoder_parameters(&parameters);
  if (opj_setup_decoder(codec.get(), &parameters) == OPJ_FALSE) {
    return std::nullopt;
  }

  MemoryStream memory{&data, 0};
  const std::unique_ptr<opj_stream_t, decltype(&opj_stream_destroy)> stream(
      opj_stream_default_create(OPJ_TRUE), &opj_stream_destroy);
  if (stream == nullptr) {
    return std::nullopt;
  }
  opj_stream_set_user_data(stream.get(), &memory, nullptr);
  opj_stream_set_user_data_length(stream.get(), data.size());
  opj_stream_set_read_function(stream.get(), stream_read);
  opj_stream_set_skip_function(stream.get(), stream_skip);
  opj_stream_set_seek_function(stream.get(), stream_seek);

  opj_image_t *raw_image = nullptr;
  if (opj_read_header(stream.get(), codec.get(), &raw_image) == OPJ_FALSE) {
    opj_image_destroy(raw_image);
    return std::nullopt;
  }
  const std::unique_ptr<opj_image_t, decltype(&opj_image_destroy)> image(
      raw_image, &opj_image_destroy);
  if (opj_decode(codec.get(), stream.get(), image.get()) == OPJ_FALSE ||
      opj_end_decompress(codec.get(), stream.get()) == OPJ_FALSE) {
    return std::nullopt;
  }

  const std::int32_t width = static_cast<std::int32_t>(image->x1) -
                             static_cast<std::int32_t>(image->x0);
  const std::int32_t height = static_cast<std::int32_t>(image->y1) -
                              static_cast<std::int32_t>(image->y0);
  if (width <= 0 || height <= 0 || image->numcomps == 0) {
    return std::nullopt;
  }
  for (std::uint32_t i = 0; i < image->numcomps; ++i) {
    const opj_image_comp_t &comp = image->comps[i];
    if (comp.data == nullptr || comp.w == 0 || comp.h == 0 || comp.prec == 0 ||
        comp.prec > 16) {
      return std::nullopt;
    }
  }

  // A component the `cdef` box marks as opacity is the alpha plane; the rest
  // are colour, in codestream order.
  std::vector<std::uint32_t> colour;
  std::optional<std::uint32_t> alpha_index;
  for (std::uint32_t i = 0; i < image->numcomps; ++i) {
    if (image->comps[i].alpha != 0 && !alpha_index.has_value()) {
      alpha_index = i;
    } else {
      colour.push_back(i);
    }
  }
  if (colour.empty() || colour.size() > 4) {
    return std::nullopt;
  }

  JpxImage result;
  result.width = width;
  result.height = height;
  result.components = static_cast<std::int32_t>(colour.size());
  result.samples.resize(static_cast<std::size_t>(width) * height *
                        colour.size());
  if (alpha_index.has_value()) {
    result.alpha.resize(static_cast<std::size_t>(width) * height);
  }

  std::size_t out = 0;
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      for (const std::uint32_t c : colour) {
        result.samples[out++] =
            static_cast<char>(sample_at(image->comps[c], x, y));
      }
      if (alpha_index.has_value()) {
        result.alpha[static_cast<std::size_t>(y) * width + x] =
            sample_at(image->comps[*alpha_index], x, y);
      }
    }
  }

  if (image->color_space == OPJ_CLRSPC_SYCC && result.components == 3) {
    sycc_to_rgb(result.samples);
  }

  return result;
}

} // namespace odr::internal
