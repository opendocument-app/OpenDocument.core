#include <odr/internal/pdf/pdf_shading.hpp>

#include <odr/internal/pdf/pdf_object.hpp>

#include <cstddef>

namespace odr::internal::pdf {

namespace {

/// Read a numeric array entry as doubles ([] when absent or not an array).
std::vector<double> read_numbers(const Dictionary &dict, const std::string &key,
                                 const ShadingContext &context) {
  std::vector<double> result;
  const Object value = context.resolve(dict.get(key));
  if (value.is_array()) {
    for (const Object &item : value.as_array()) {
      result.push_back(item.as_real());
    }
  }
  return result;
}

/// Parse the `/Function` of a shading: either one function or an array of
/// single-output functions (one per colour component, ISO 32000-1 8.7.4.5.2).
std::vector<std::shared_ptr<Function>>
parse_shading_functions(const Object &function, const ShadingContext &context) {
  FunctionContext function_context;
  function_context.resolve = context.resolve;
  function_context.load_stream = context.load_stream;

  std::vector<std::shared_ptr<Function>> functions;
  const Object resolved = context.resolve(function);
  if (resolved.is_array()) {
    for (const Object &item : resolved.as_array()) {
      functions.push_back(parse_function(item, function_context));
    }
  } else {
    functions.push_back(parse_function(resolved, function_context));
  }
  // A null entry (an unsupported function type) makes the whole shading
  // unusable: sampling would yield wrong colours silently.
  for (const auto &f : functions) {
    if (f == nullptr) {
      return {};
    }
  }
  return functions;
}

/// Evaluate the shading's tint functions at `t`, concatenating their outputs
/// into the colour-component vector the colour space expects.
std::vector<double>
eval_components(const std::vector<std::shared_ptr<Function>> &functions,
                double t) {
  std::vector<double> components;
  for (const auto &function : functions) {
    std::vector<double> out = function->eval({t});
    components.insert(components.end(), out.begin(), out.end());
  }
  return components;
}

} // namespace

} // namespace odr::internal::pdf

namespace odr::internal {

std::shared_ptr<pdf::Shading>
pdf::parse_shading(const Object &object, const ShadingContext &context,
                   const ColorSpaceContext &color_context) {
  const Object resolved = context.resolve(object);
  if (!resolved.is_dictionary()) {
    return nullptr;
  }
  const Dictionary &dict = resolved.as_dictionary();

  if (!dict.get("ShadingType").is_integer()) {
    return nullptr;
  }
  const auto type =
      static_cast<std::int32_t>(dict.get("ShadingType").as_integer());
  if (type != 2 && type != 3) {
    return nullptr; // function-based / mesh shadings are a later stage
  }

  const std::shared_ptr<ColorSpaceDef> color_space =
      parse_color_space(dict.get("ColorSpace"), color_context);
  if (color_space == nullptr) {
    return nullptr;
  }

  const std::vector<double> coords = read_numbers(dict, "Coords", context);
  const std::size_t need = type == 2 ? 4 : 6;
  if (coords.size() < need) {
    return nullptr;
  }

  const std::vector<std::shared_ptr<Function>> functions =
      parse_shading_functions(dict.get("Function"), context);
  if (functions.empty()) {
    return nullptr;
  }

  auto shading = std::make_shared<Shading>();
  shading->type = type;
  for (std::size_t i = 0; i < need; ++i) {
    shading->coords[i] = coords[i];
  }

  const std::vector<double> domain = read_numbers(dict, "Domain", context);
  if (domain.size() >= 2) {
    shading->domain = {domain[0], domain[1]};
  }

  const Object extend = context.resolve(dict.get("Extend"));
  if (extend.is_array() && extend.as_array().size() >= 2) {
    shading->extend = {extend.as_array()[0].as_bool_opt().value_or(false),
                       extend.as_array()[1].as_bool_opt().value_or(false)};
  }

  // Sample the tint function(s) across the domain into colour stops. A fixed
  // count captures non-linear (stitching/sampled/PostScript) functions well
  // enough for an SVG gradient; an exponential one is reproduced near-exactly.
  constexpr std::size_t sample_count = 32;
  const double t0 = shading->domain[0];
  const double t1 = shading->domain[1];
  shading->stops.reserve(sample_count);
  for (std::size_t i = 0; i < sample_count; ++i) {
    const double f = static_cast<double>(i) / (sample_count - 1);
    const double t = t0 + (t1 - t0) * f;
    shading->stops.push_back(
        GradientStop{f, color_space->to_rgb(eval_components(functions, t))});
  }

  const std::vector<double> background =
      read_numbers(dict, "Background", context);
  if (!background.empty()) {
    shading->has_background = true;
    shading->background = color_space->to_rgb(background);
  }

  const std::vector<double> bbox = read_numbers(dict, "BBox", context);
  if (bbox.size() >= 4) {
    shading->has_bbox = true;
    shading->bbox = {bbox[0], bbox[1], bbox[2], bbox[3]};
  }

  return shading;
}

} // namespace odr::internal
