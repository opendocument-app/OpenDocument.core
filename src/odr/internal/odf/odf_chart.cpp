#include <odr/internal/odf/odf_chart.hpp>

#include <odr/internal/odf/odf_geometry.hpp>
#include <odr/internal/util/number_util.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace odr::internal::odf {

namespace {

/// The chart's own coordinates: 1/100 mm, which is what `svg:width` and the
/// plot area's box reduce to.
struct Box final {
  double x{0};
  double y{0};
  double width{0};
  double height{0};
};

struct Series final {
  std::string label;
  std::vector<std::optional<double>> values;
  std::string colour;
};

/// What libreoffice paints an unstyled series with, in order.
constexpr std::array<std::string_view, 12> default_colours{
    "#004586", "#ff420e", "#ffd320", "#579d1c", "#7e0021", "#83caff",
    "#314004", "#aecf00", "#4b1f6f", "#ff950e", "#c5000b", "#0084d1"};

/// 1/100 mm per typographic point, the unit chart text is sized in.
constexpr double units_per_point = 2540.0 / 72.0;

/// The size a legend entry and an axis label are drawn at, and roughly what
/// one of their characters is wide in ems.
constexpr double label_points = 9;
constexpr double character_width = 0.55;

std::string number(const double value) {
  if (!std::isfinite(value)) {
    throw std::overflow_error("nonfinite chart coordinate");
  }
  return util::number::to_string_significant(value == 0 ? 0 : value, 6);
}

double read_length(const pugi::xml_attribute attribute,
                   const double fallback = 0) {
  return read_hundredth_millimetres(attribute).value_or(fallback);
}

void collect_text(const pugi::xml_node node, std::string &out) {
  for (const pugi::xml_node child : node.children()) {
    if (child.type() == pugi::node_pcdata) {
      out += child.value();
    } else {
      collect_text(child, out);
    }
  }
}

/// The text of a `text:p` run, which is all a chart title or label is.
std::string read_text(const pugi::xml_node node) {
  std::string result;
  for (const pugi::xml_node paragraph : node.children("text:p")) {
    collect_text(paragraph, result);
  }
  // A title is written with its trailing newline; one line is all we draw.
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r' ||
                             result.back() == ' ')) {
    result.pop_back();
  }
  return result;
}

/// A step that lands on 1, 2 or 5 times a power of ten, so the axis reads.
double nice_step(const double rough) {
  if (rough <= 0) {
    return 1;
  }
  const double magnitude = std::pow(10, std::floor(std::log10(rough)));
  const double normalised = rough / magnitude;
  if (normalised <= 1) {
    return magnitude;
  }
  if (normalised <= 2) {
    return 2 * magnitude;
  }
  if (normalised <= 5) {
    return 5 * magnitude;
  }
  return 10 * magnitude;
}

/// Reads `<office:chart>` and draws it.
class ChartWriter {
public:
  explicit ChartWriter(const pugi::xml_node content_root)
      : m_chart{content_root.child("office:body")
                    .child("office:chart")
                    .child("chart:chart")} {
    for (const pugi::xml_node style :
         content_root.child("office:automatic-styles")
             .children("style:style")) {
      m_styles.emplace(style.attribute("style:name").value(), style);
    }
  }

  [[nodiscard]] std::optional<std::string> render() {
    if (!m_chart) {
      return {};
    }
    m_size.width = read_length(m_chart.attribute("svg:width"));
    m_size.height = read_length(m_chart.attribute("svg:height"));
    if (!valid_box(m_size)) {
      return {};
    }

    const pugi::xml_node plot_area = m_chart.child("chart:plot-area");
    read_class();
    read_plot_box(plot_area);
    if (!valid_box(m_plot) || !read_data(plot_area) || m_series.empty()) {
      return {};
    }

    open();
    write_title();
    write_legend();
    if (m_pie) {
      write_pie();
    } else {
      write_value_axis();
      write_category_axis();
      write_series();
    }
    m_out += "</svg>";
    return m_out;
  }

private:
  pugi::xml_node m_chart;
  std::unordered_map<std::string, pugi::xml_node> m_styles;

  Box m_size;
  Box m_plot;
  std::string m_class;
  bool m_bars{false};
  bool m_area{false};
  bool m_symbols{false};
  bool m_pie{false};

  std::vector<std::string> m_categories;
  std::vector<Series> m_series;
  double m_minimum{0};
  double m_maximum{0};
  double m_step{1};

  std::string m_out;

  [[nodiscard]] pugi::xml_node style_of(const pugi::xml_node node) const {
    const auto it = m_styles.find(node.attribute("chart:style-name").value());
    return it == m_styles.end() ? pugi::xml_node() : it->second;
  }

  static bool valid_box(const Box &box) {
    return std::isfinite(box.x) && std::isfinite(box.y) &&
           std::isfinite(box.width) && std::isfinite(box.height) &&
           box.width > 0 && box.height > 0;
  }

  void read_class() {
    m_class = m_chart.attribute("chart:class").value();
    m_bars = m_class == "chart:bar";
    m_area = m_class == "chart:area";
    m_pie = m_class == "chart:circle" || m_class == "chart:ring";
    m_symbols =
        m_class == "chart:scatter" || style_of(m_chart.child("chart:plot-area"))
                                          .child("style:chart-properties")
                                          .attribute("chart:symbol-type");
  }

  /// `chartooo:coordinate-region` is the region the data is drawn in, which the
  /// plot area's own box only bounds.
  void read_plot_box(const pugi::xml_node plot_area) {
    pugi::xml_node box = plot_area.child("chartooo:coordinate-region");
    if (!box) {
      box = plot_area;
    }
    m_plot.x = read_length(box.attribute("svg:x"));
    m_plot.y = read_length(box.attribute("svg:y"));
    m_plot.width = read_length(box.attribute("svg:width"), m_size.width);
    m_plot.height = read_length(box.attribute("svg:height"), m_size.height);
  }

  /// The `local-table` carries the plotted values; its header columns are the
  /// categories and its header rows the series labels.
  bool read_data(const pugi::xml_node plot_area) {
    const pugi::xml_node table = m_chart.find_child_by_attribute(
        "table:table", "table:name", "local-table");
    if (!table) {
      return false;
    }

    const std::size_t leading = std::max<std::size_t>(
        1, count_cells(table.child("table:table-header-columns")
                           .child("table:table-column")));
    if (leading > max_cells) {
      return false;
    }

    std::vector<std::string> labels;
    if (const pugi::xml_node header =
            table.child("table:table-header-rows").child("table:table-row")) {
      const auto cells = read_row(header);
      if (!cells) {
        return false;
      }
      labels = *cells | std::views::transform(read_text) |
               std::ranges::to<std::vector<std::string>>();
    }

    std::vector<std::vector<std::optional<double>>> columns;
    for (const pugi::xml_node row :
         table.child("table:table-rows").children("table:table-row")) {
      const auto cells = read_row(row);
      const std::uint32_t repeated =
          row.attribute("table:number-rows-repeated").as_uint(1);
      if (!cells || repeated == 0 ||
          repeated > max_cells - m_categories.size()) {
        return false;
      }
      const std::size_t rows = m_categories.size() + repeated;
      const std::size_t width =
          std::max(columns.size(),
                   cells->size() > leading ? cells->size() - leading : 0);
      if (width > max_cells / rows) {
        return false;
      }
      columns.resize(width);
      for (std::size_t i = 0; i < width; ++i) {
        columns[i].resize(m_categories.size());
        columns[i].insert(columns[i].end(), repeated,
                          leading + i < cells->size()
                              ? read_value((*cells)[leading + i])
                              : std::nullopt);
      }
      m_categories.insert(m_categories.end(), repeated,
                          leading <= cells->size()
                              ? read_text((*cells)[leading - 1])
                              : std::string());
    }

    std::size_t index = 0;
    for (const pugi::xml_node series : plot_area.children("chart:series")) {
      if (index >= columns.size()) {
        break;
      }
      Series result;
      result.values = columns[index];
      if (leading + index < labels.size()) {
        result.label = labels[leading + index];
      }
      result.colour = read_colour(series, index);
      m_series.push_back(std::move(result));
      ++index;
    }
    // A chart with no `chart:series` at all still has its table.
    if (m_series.empty()) {
      for (; index < columns.size(); ++index) {
        m_series.push_back({.label = leading + index < labels.size()
                                         ? labels[leading + index]
                                         : std::string(),
                            .values = std::move(columns[index]),
                            .colour = std::string(colour_at(index))});
      }
    }

    trim();
    return m_pie || read_range();
  }

  static constexpr std::size_t max_cells = 1048576;

  [[nodiscard]] static std::size_t count_cells(const pugi::xml_node column) {
    std::size_t result = 0;
    for (pugi::xml_node node = column; node;
         node = node.next_sibling("table:table-column")) {
      const std::size_t repeated =
          node.attribute("table:number-columns-repeated").as_uint(1);
      if (repeated > max_cells - result) {
        return max_cells + 1;
      }
      result += repeated;
    }
    return result;
  }

  [[nodiscard]] static std::optional<std::vector<pugi::xml_node>>
  read_row(const pugi::xml_node row) {
    std::vector<pugi::xml_node> result;
    for (const pugi::xml_node cell : row.children()) {
      if (std::strcmp(cell.name(), "table:table-cell") != 0 &&
          std::strcmp(cell.name(), "table:covered-table-cell") != 0) {
        continue;
      }
      const auto repeated =
          cell.attribute("table:number-columns-repeated").as_uint(1);
      if (repeated == 0 || repeated > max_cells - result.size()) {
        return std::nullopt;
      }
      result.insert(result.end(), repeated, cell);
    }
    return result;
  }

  /// A missing data point is written `office:value="NaN"`.
  [[nodiscard]] static std::optional<double>
  read_value(const pugi::xml_node cell) {
    const pugi::xml_attribute value = cell.attribute("office:value");
    if (!value) {
      return {};
    }
    return util::number::parse(value.value());
  }

  /// The table is written to the chart's full range, so it ends in rows that
  /// carry neither a category nor a value.
  void trim() {
    while (!m_categories.empty()) {
      const std::size_t last = m_categories.size() - 1;
      if (!m_categories[last].empty()) {
        return;
      }
      for (const Series &series : m_series) {
        if (last < series.values.size() && series.values[last].has_value()) {
          return;
        }
      }
      m_categories.pop_back();
      for (Series &series : m_series) {
        if (last < series.values.size()) {
          series.values.pop_back();
        }
      }
    }
  }

  [[nodiscard]] static std::string_view colour_at(const std::size_t index) {
    return default_colours[index % default_colours.size()];
  }

  [[nodiscard]] std::string read_colour(const pugi::xml_node series,
                                        const std::size_t index) const {
    const pugi::xml_node properties =
        style_of(series).child("style:graphic-properties");
    for (const char *name : {"draw:fill-color", "svg:stroke-color"}) {
      if (const pugi::xml_attribute colour = properties.attribute(name);
          colour && colour.value()[0] == '#') {
        return colour.value();
      }
    }
    return std::string(colour_at(index));
  }

  bool read_range() {
    bool empty = true;
    for (const Series &series : m_series) {
      for (const std::optional<double> value : series.values) {
        if (!value.has_value()) {
          continue;
        }
        m_minimum = empty ? *value : std::min(m_minimum, *value);
        m_maximum = empty ? *value : std::max(m_maximum, *value);
        empty = false;
      }
    }
    if (empty) {
      return false;
    }
    // A bar or an area is measured from zero, or it lies about its size.
    if (m_bars || m_area) {
      m_minimum = std::min(m_minimum, 0.0);
      m_maximum = std::max(m_maximum, 0.0);
    }
    if (m_maximum == m_minimum) {
      // a large value absorbs a padding of 1
      m_maximum = m_minimum + std::max(1.0, std::abs(m_minimum) * 1e-6);
    }
    m_step = nice_step((m_maximum - m_minimum) / 5);
    if (!std::isfinite(m_step) || m_step <= 0) {
      return false;
    }
    m_minimum = std::floor(m_minimum / m_step) * m_step;
    m_maximum = std::ceil(m_maximum / m_step) * m_step;
    const double span = m_maximum - m_minimum;
    return std::isfinite(span) && span > 0;
  }

  [[nodiscard]] double value_to_y(const double value) const {
    return m_plot.y +
           m_plot.height * (1 - (value - m_minimum) / (m_maximum - m_minimum));
  }

  void open() {
    m_out += R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1")";
    m_out += " viewBox=\"0 0 " + number(m_size.width) + " " +
             number(m_size.height) + "\"";
    m_out += " width=\"" + number(m_size.width / 1000) + "cm\"";
    m_out += " height=\"" + number(m_size.height / 1000) + "cm\">";
    m_out += "<rect x=\"0\" y=\"0\" width=\"" + number(m_size.width) +
             "\" height=\"" + number(m_size.height) + "\" fill=\"#ffffff\"/>";
    m_out += "<rect x=\"" + number(m_plot.x) + "\" y=\"" + number(m_plot.y) +
             "\" width=\"" + number(m_plot.width) + "\" height=\"" +
             number(m_plot.height) + "\" fill=\"#ffffff\" stroke=\"#b3b3b3\"/>";
  }

  void write_text(const double x, const double y, const std::string &text,
                  const double points, const char *anchor) {
    if (text.empty()) {
      return;
    }
    m_out += "<text x=\"" + number(x) + "\" y=\"" + number(y) +
             "\" font-family=\"sans-serif\" font-size=\"" +
             number(points * units_per_point) + "\" text-anchor=\"" + anchor +
             "\" fill=\"#000000\">";
    m_out += xml::escape_text(text);
    m_out += "</text>";
  }

  void write_title() {
    write_text(m_size.width / 2,
               read_length(m_chart.child("chart:title").attribute("svg:y"),
                           m_size.height * 0.06) +
                   13 * units_per_point,
               read_text(m_chart.child("chart:title")), 13, "middle");
    write_text(m_size.width / 2,
               read_length(m_chart.child("chart:subtitle").attribute("svg:y"),
                           m_size.height * 0.12) +
                   10 * units_per_point,
               read_text(m_chart.child("chart:subtitle")), 10, "middle");
  }

  void write_legend() {
    const pugi::xml_node legend = m_chart.child("chart:legend");
    if (!legend) {
      return;
    }
    const double x =
        read_length(legend.attribute("svg:x"), m_plot.x + m_plot.width + 200);
    double y = read_length(legend.attribute("svg:y"), m_plot.y);
    const double size = label_points * units_per_point;
    for (const Series &series : m_series) {
      m_out += "<rect x=\"" + number(x) + "\" y=\"" + number(y) +
               "\" width=\"" + number(size) + "\" height=\"" + number(size) +
               "\" fill=\"" + xml::escape_attribute(series.colour) + "\"/>";
      write_text(x + size * 1.5, y + size * 0.85, series.label, label_points,
                 "start");
      y += size * 1.6;
    }
  }

  void write_value_axis() {
    const double intervals = std::round((m_maximum - m_minimum) / m_step);
    if (!std::isfinite(intervals) || intervals < 0 || intervals > 10) {
      throw std::overflow_error("invalid chart axis intervals");
    }
    const auto ticks = static_cast<std::int32_t>(intervals);
    for (std::int32_t tick = 0; tick <= ticks; ++tick) {
      const double value = m_minimum + m_step * tick;
      const double y = value_to_y(value);
      m_out += "<line x1=\"" + number(m_plot.x) + "\" y1=\"" + number(y) +
               "\" x2=\"" + number(m_plot.x + m_plot.width) + "\" y2=\"" +
               number(y) + "\" stroke=\"#b3b3b3\"/>";
      write_text(m_plot.x - 100, y + 3 * units_per_point / 2,
                 util::number::to_string_significant(value, 6), label_points,
                 "end");
    }
  }

  void write_category_axis() {
    if (m_categories.empty()) {
      return;
    }
    // Every nth, n chosen so the widest label still has room beside it.
    std::size_t longest = 1;
    for (const std::string &category : m_categories) {
      longest = std::max(longest, category.size());
    }
    const double width = static_cast<double>(longest) * label_points *
                         units_per_point * character_width;
    const double labels = static_cast<double>(m_categories.size());
    const double spacing = std::ceil(width / m_plot.width * labels);
    const auto stride =
        static_cast<std::size_t>(std::max(1.0, std::min(spacing, labels)));
    for (std::size_t i = 0; i < m_categories.size(); i += stride) {
      write_text(category_centre(i),
                 m_plot.y + m_plot.height + 10 * units_per_point,
                 m_categories[i], label_points, "middle");
    }
  }

  [[nodiscard]] double category_centre(const std::size_t index) const {
    return m_plot.x + m_plot.width * (static_cast<double>(index) + 0.5) /
                          static_cast<double>(
                              std::max<std::size_t>(1, m_categories.size()));
  }

  void write_series() {
    if (m_bars) {
      write_bars();
      return;
    }
    for (const Series &series : m_series) {
      write_line(series);
    }
  }

  void write_bars() {
    const auto count = static_cast<double>(m_series.size());
    const double band =
        m_plot.width /
        static_cast<double>(std::max<std::size_t>(1, m_categories.size()));
    const double width = band * 0.8 / count;
    const double zero = value_to_y(std::clamp(0.0, m_minimum, m_maximum));

    for (std::size_t s = 0; s < m_series.size(); ++s) {
      for (std::size_t i = 0; i < m_series[s].values.size(); ++i) {
        const std::optional<double> value = m_series[s].values[i];
        if (!value.has_value()) {
          continue;
        }
        const double y = value_to_y(*value);
        const double x =
            category_centre(i) - band * 0.4 + width * static_cast<double>(s);
        m_out += "<rect x=\"" + number(x) + "\" y=\"" +
                 number(std::min(y, zero)) + "\" width=\"" + number(width) +
                 "\" height=\"" + number(std::abs(zero - y)) + "\" fill=\"" +
                 xml::escape_attribute(m_series[s].colour) + "\"/>";
      }
    }
  }

  void write_line(const Series &series) {
    std::string points;
    for (std::size_t i = 0; i < series.values.size(); ++i) {
      const std::optional<double> value = series.values[i];
      if (!value.has_value()) {
        continue;
      }
      points += points.empty() ? "M " : " L ";
      points += number(category_centre(i));
      points += ' ';
      points += number(value_to_y(*value));
    }
    if (points.empty()) {
      return;
    }

    if (m_area) {
      const double zero = value_to_y(std::clamp(0.0, m_minimum, m_maximum));
      m_out += "<path d=\"" + points + " L " +
               number(category_centre(series.values.size() - 1)) + " " +
               number(zero) + " L " + number(category_centre(0)) + " " +
               number(zero) + " Z\" fill=\"" +
               xml::escape_attribute(series.colour) +
               "\" fill-opacity=\"0.6\"/>";
    }
    m_out += "<path d=\"" + points + "\" fill=\"none\" stroke=\"" +
             xml::escape_attribute(series.colour) + "\" stroke-width=\"" +
             number(units_per_point) + "\"/>";

    if (!m_symbols) {
      return;
    }
    for (std::size_t i = 0; i < series.values.size(); ++i) {
      const std::optional<double> value = series.values[i];
      if (!value.has_value()) {
        continue;
      }
      m_out += "<circle cx=\"" + number(category_centre(i)) + "\" cy=\"" +
               number(value_to_y(*value)) + "\" r=\"" +
               number(units_per_point * 1.5) + "\" fill=\"" +
               xml::escape_attribute(series.colour) + "\"/>";
    }
  }

  /// A pie plots one series, its slices the categories.
  void write_pie() {
    const Series &series = m_series.front();
    double maximum = 0;
    for (const std::optional<double> value : series.values) {
      maximum = std::max(maximum, value.value_or(0));
    }
    if (maximum <= 0) {
      return;
    }
    double total = 0;
    for (const std::optional<double> value : series.values) {
      total += std::max(0.0, value.value_or(0)) / maximum;
    }

    const double cx = m_plot.x + m_plot.width / 2;
    const double cy = m_plot.y + m_plot.height / 2;
    const double radius = std::min(m_plot.width, m_plot.height) / 2;
    const double inner = m_class == "chart:ring" ? radius / 2 : 0;

    double from = -90;
    for (std::size_t i = 0; i < series.values.size(); ++i) {
      const double value = series.values[i].value_or(0);
      if (value <= 0) {
        continue;
      }
      const double to = from + 360 * (value / maximum) / total;
      m_out += "<path d=\"" + slice(cx, cy, radius, inner, from, to) +
               "\" fill=\"" + std::string(colour_at(i)) + "\"/>";
      from = to;
    }
  }

  [[nodiscard]] static std::string slice(const double cx, const double cy,
                                         const double radius,
                                         const double inner, const double from,
                                         const double to) {
    const auto point = [&](const double degrees, const double r) {
      const double radians = degrees * std::numbers::pi / 180;
      return number(cx + r * std::cos(radians)) + " " +
             number(cy + r * std::sin(radians));
    };
    const double middle = (from + to) / 2;
    const auto arc = [&](const double r, const char *direction,
                         const double end) {
      return " A " + number(r) + " " + number(r) + " 0 0 " + direction + " " +
             point(end, r);
    };
    std::string result = "M " + point(from, radius) + arc(radius, "1", middle) +
                         arc(radius, "1", to);
    if (inner > 0) {
      result += " L " + point(to, inner) + arc(inner, "0", middle) +
                arc(inner, "0", from);
    } else {
      result += " L " + number(cx) + " " + number(cy);
    }
    return result + " Z";
  }
};

} // namespace

} // namespace odr::internal::odf

namespace odr::internal {

std::optional<std::string>
odf::render_chart(const pugi::xml_node content_root) {
  try {
    return odf::ChartWriter(content_root).render();
  } catch (const std::overflow_error &) {
    return std::nullopt;
  }
}

} // namespace odr::internal
