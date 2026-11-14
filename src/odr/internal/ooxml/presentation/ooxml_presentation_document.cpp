#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/element_adapter.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/common/style.hpp>
#include <odr/internal/ooxml/ooxml_util.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_parser.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_style.hpp>
#include <odr/internal/xml/xml_tree_edit.hpp>
#include <odr/internal/xml/xml_util.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

namespace odr::internal::ooxml::presentation {

namespace {
std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(Document &document, ElementRegistry &registry);

} // namespace

Document::Document(std::shared_ptr<abstract::ReadableFilesystem> files,
                   std::shared_ptr<zip::util::Archive> archive)
    : internal::Document(FileType::office_open_xml_presentation,
                         DocumentType::presentation, std::move(files),
                         EncryptionState::not_encrypted, std::move(archive)) {
  m_document_xml = xml::parse(*m_files, AbsPath("/ppt/presentation.xml"));

  // Only the parts the slide-id list names: a package may relate anything at
  // all to the presentation, and a Google Slides export relates a protobuf.
  m_relations = parse_relationships(*m_files, AbsPath("/ppt/presentation.xml"));
  const Relations &relations = m_relations;
  std::vector<AbsPath> slides;
  for (const pugi::xml_node slide_id : m_document_xml.document_element()
                                           .child("p:sldIdLst")
                                           .children("p:sldId")) {
    const std::string id = slide_id.attribute("r:id").value();
    AbsPath slide_path =
        resolve_part_path(AbsPath("/ppt/presentation.xml"), relations.at(id));
    m_slides_xml[id] = xml::parse(*m_files, slide_path);
    m_slide_ids_by_path[slide_path] = id;
    slides.push_back(std::move(slide_path));
  }

  // ECMA-376 default slide size when p:sldSz is absent.
  m_slide_layout.width = Measure("10 in");
  m_slide_layout.height = Measure("7.5 in");
  if (const pugi::xml_node slide_size =
          m_document_xml.document_element().child("p:sldSz")) {
    if (const std::optional<Measure> width =
            read_emus_attribute(slide_size.attribute("cx"));
        width.has_value()) {
      m_slide_layout.width = width;
    }
    if (const std::optional<Measure> height =
            read_emus_attribute(slide_size.attribute("cy"));
        height.has_value()) {
      m_slide_layout.height = height;
    }
  }

  const ParseContext parse_context(m_slides_xml);
  m_root_element = parse_tree(m_element_registry, parse_context,
                              m_document_xml.document_element());

  load_slide_styles_(slides);

  m_element_adapter = create_element_adapter(*this, m_element_registry);
}

bool Document::is_editable() const noexcept { return true; }

bool Document::is_savable(const bool encrypted) const noexcept {
  return !encrypted && !is_decrypted();
}

/// Only the slides are re-serialised; everything else is copied through as
/// bytes, so a part we never parsed survives untouched.
void Document::save(std::ostream &out) const {
  if (!is_savable(false)) {
    throw UnsupportedOperation();
  }

  // TODO this would decrypt/inflate and encrypt/deflate again
  zip::ZipArchive archive;

  const auto write = [&](const AbsPath &path, const pugi::xml_document &xml,
                         const bool declaration) {
    // TODO stream
    std::stringstream content;
    if (declaration) {
      // pugixml is never asked to parse the declaration, so it writes none
      content << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>)";
    }
    xml.print(content, "", pugi::format_raw);
    archive.insert_file(std::end(archive), path.rebase(AbsPath("/")),
                        std::make_shared<MemoryFile>(content.str()));
  };

  for (const auto &[path, id] : m_slide_ids_by_path) {
    write(path, m_slides_xml.at(id), false);
  }
  for (const auto &[path, xml] : m_written_parts) {
    write(path, xml, true);
  }
  if (m_presentation_changed) {
    write(AbsPath("/ppt/presentation.xml"), m_document_xml, true);
  }

  for (auto walker = m_files->file_walker(AbsPath("/")); !walker->end();
       walker->next()) {
    const AbsPath &abs_path = walker->path();
    if (m_slide_ids_by_path.contains(abs_path) ||
        m_written_parts.contains(abs_path) ||
        (m_presentation_changed &&
         abs_path == AbsPath("/ppt/presentation.xml"))) {
      continue;
    }
    const RelPath rel_path = abs_path.rebase(AbsPath("/"));
    if (walker->is_directory()) {
      archive.insert_directory(std::end(archive), rel_path);
      continue;
    }
    archive.insert_file(std::end(archive), rel_path, m_files->open(abs_path));
  }

  archive.save(out);
}

void Document::save(std::ostream & /*out*/, const char * /*password*/) const {
  throw UnsupportedOperation();
}

/// slide → layout → master → theme; the master's `p:clrMap` says which slot
/// each name stands for.
void Document::load_slide_styles_(const std::vector<AbsPath> &slides) {
  if (m_root_element == null_element_id) {
    return;
  }

  // `parse_presentation_children` appends one slide per `p:sldId`, so the
  // root's children carry the same order `slides` was built in.
  ElementIdentifier slide_id =
      m_element_registry.element_at(m_root_element).first_child_id;
  for (const AbsPath &slide_path : slides) {
    if (slide_id == null_element_id) {
      break;
    }
    const ElementIdentifier current_id = slide_id;
    const pugi::xml_node slide_node =
        m_element_registry.element_at(current_id).node;
    slide_id = m_element_registry.element_at(current_id).next_sibling_id;

    const std::optional<AbsPath> layout_path =
        parse_relationship_target(*m_files, slide_path, "slideLayout");
    const LayoutStyle no_layout;
    const LayoutStyle &layout_style =
        layout_path.has_value() ? m_layout_styles.get(*m_files, *layout_path)
                                : no_layout;
    if (layout_style.layout) {
      m_slide_styles[current_id] = &layout_style;
    }

    const ColorScheme *color_scheme = layout_style.theme == nullptr
                                          ? nullptr
                                          : &layout_style.theme->color_scheme;
    std::optional<Color> background = layout_style.background;
    if (const std::optional<std::optional<Color>> stated =
            read_background_color(slide_node, color_scheme);
        stated.has_value()) {
      background = *stated;
    }
    if (background.has_value()) {
      m_slide_backgrounds[current_id] = *background;
    }
  }
}

pugi::xml_node Document::written_part_(const AbsPath &path) {
  if (const auto it = m_written_parts.find(path); it != m_written_parts.end()) {
    return it->second.document_element();
  }
  if (!m_files->is_file(path)) {
    return {};
  }
  pugi::xml_document &xml = m_written_parts[path];
  xml = xml::parse(*m_files, path);
  return xml.document_element();
}

std::vector<std::pair<std::string, AbsPath>> Document::slide_layouts() const {
  const AbsPath presentation_path("/ppt/presentation.xml");
  std::vector<std::pair<std::string, AbsPath>> result;
  for (const pugi::xml_node master_id : m_document_xml.document_element()
                                            .child("p:sldMasterIdLst")
                                            .children("p:sldMasterId")) {
    const auto master_target =
        m_relations.find(master_id.attribute("r:id").value());
    if (master_target == m_relations.end()) {
      continue;
    }
    const AbsPath master_path =
        resolve_part_path(presentation_path, master_target->second);
    if (!m_files->is_file(master_path)) {
      continue;
    }
    const pugi::xml_document master = xml::parse(*m_files, master_path);
    const Relations master_relations =
        parse_relationships(*m_files, master_path);
    for (const pugi::xml_node layout_id : master.document_element()
                                              .child("p:sldLayoutIdLst")
                                              .children("p:sldLayoutId")) {
      const auto layout_target =
          master_relations.find(layout_id.attribute("r:id").value());
      if (layout_target == master_relations.end()) {
        continue;
      }
      AbsPath layout_path =
          resolve_part_path(master_path, layout_target->second);
      if (!m_files->is_file(layout_path)) {
        continue;
      }
      std::string name = xml::parse(*m_files, layout_path)
                             .document_element()
                             .child("p:cSld")
                             .attribute("name")
                             .value();
      if (name.empty()) {
        name = "Layout " + std::to_string(result.size() + 1);
      }
      // a second master often repeats the first one's names
      const auto count = std::ranges::count_if(result, [&](const auto &entry) {
        return entry.first == name || entry.first.starts_with(name + " (");
      });
      if (count > 0) {
        name += " (" + std::to_string(count + 1) + ")";
      }
      result.emplace_back(std::move(name), std::move(layout_path));
    }
  }
  return result;
}

ElementIdentifier Document::insert_slide(const std::uint32_t index,
                                         const std::uint32_t layout) {
  const AbsPath presentation_path("/ppt/presentation.xml");
  const AbsPath layout_path = slide_layouts().at(layout).second;
  pugi::xml_node relationships =
      written_part_(AbsPath("/ppt/_rels/presentation.xml.rels"));
  pugi::xml_node types = written_part_(AbsPath("/[Content_Types].xml"));
  if (!relationships || !types) {
    throw UnsupportedOperation();
  }

  std::string file_name;
  std::string target;
  AbsPath slide_path("/");
  for (std::uint32_t number = 1;; ++number) {
    file_name = "slide" + std::to_string(number) + ".xml";
    target = "slides/" + file_name;
    slide_path = resolve_part_path(presentation_path, target);
    if (!m_files->is_file(slide_path) &&
        !m_slide_ids_by_path.contains(slide_path)) {
      break;
    }
  }

  std::string id;
  for (std::uint32_t number = 1;; ++number) {
    id = "rId" + std::to_string(number);
    if (!relationships.find_child_by_attribute("Relationship", "Id",
                                               id.c_str()) &&
        !m_slides_xml.contains(id)) {
      break;
    }
  }
  pugi::xml_node relationship = relationships.append_child("Relationship");
  relationship.append_attribute("Id").set_value(id.c_str());
  relationship.append_attribute("Type").set_value(
      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
      "slide");
  relationship.append_attribute("Target").set_value(target.c_str());
  m_relations[id] = target;

  pugi::xml_node type = types.append_child("Override");
  type.append_attribute("PartName").set_value(slide_path.string().c_str());
  type.append_attribute("ContentType")
      .set_value("application/vnd.openxmlformats-officedocument.presentationml."
                 "slide+xml");

  const RelPath layout_target = layout_path.rebase(slide_path.parent());
  pugi::xml_document &slide_relationships = m_written_parts[resolve_part_path(
      presentation_path, "slides/_rels/" + file_name + ".rels")];
  pugi::xml_node slide_relationships_root =
      slide_relationships.append_child("Relationships");
  slide_relationships_root.append_attribute("xmlns").set_value(
      "http://schemas.openxmlformats.org/package/2006/relationships");
  pugi::xml_node layout_relationship =
      slide_relationships_root.append_child("Relationship");
  layout_relationship.append_attribute("Id").set_value("rId1");
  layout_relationship.append_attribute("Type").set_value(
      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
      "slideLayout");
  layout_relationship.append_attribute("Target").set_value(
      layout_target.string().c_str());

  pugi::xml_document &slide = m_slides_xml[id];
  m_slide_ids_by_path[slide_path] = id;
  write_placeholder_slide(slide, xml::parse(*m_files, layout_path));

  // [ECMA-376] 19.2.1.33: a slide id is at least 256 and unique
  pugi::xml_node root = m_document_xml.document_element();
  pugi::xml_node list = root.child("p:sldIdLst");
  if (!list) {
    pugi::xml_node before = root.child("p:handoutMasterIdLst");
    if (!before) {
      before = root.child("p:notesMasterIdLst");
    }
    if (!before) {
      before = root.child("p:sldMasterIdLst");
    }
    list = root.insert_child_after("p:sldIdLst", before);
  }
  std::uint32_t slide_number = 255;
  std::vector<pugi::xml_node> entries;
  for (const pugi::xml_node entry : list.children("p:sldId")) {
    slide_number = std::max(slide_number, entry.attribute("id").as_uint());
    entries.push_back(entry);
  }
  pugi::xml_node entry =
      index < entries.size()
          ? list.insert_child_before("p:sldId", entries[index])
          : list.append_child("p:sldId");
  entry.append_attribute("id").set_value(slide_number + 1);
  entry.append_attribute("r:id").set_value(id.c_str());
  m_presentation_changed = true;

  const ElementIdentifier slide_id = parse_tree(
      m_element_registry, ParseContext(m_slides_xml), slide.document_element());
  ElementIdentifier before =
      m_element_registry.element_at(m_root_element).first_child_id;
  for (std::uint32_t i = 0; i < index && before != null_element_id; ++i) {
    before = m_element_registry.element_at(before).next_sibling_id;
  }
  if (before != null_element_id) {
    m_element_registry.insert_sibling_before(before, slide_id);
  } else {
    m_element_registry.append_child(m_root_element, slide_id);
  }

  const LayoutStyle style =
      load_layout_style(*m_files, layout_path, m_themes, m_style_parts);
  if (style.layout) {
    m_slide_styles[slide_id] = style;
  }
  if (style.background.has_value()) {
    m_slide_backgrounds[slide_id] = *style.background;
  }
  return slide_id;
}

const LayoutStyle *
Document::slide_style(const ElementIdentifier element_id) const {
  const auto it = m_slide_styles.find(element_id);
  return it == std::end(m_slide_styles) ? nullptr : it->second;
}

pugi::xml_node Document::default_text_style() const {
  return m_document_xml.document_element().child("p:defaultTextStyle");
}

PageLayout
Document::slide_page_layout(const ElementIdentifier element_id) const {
  PageLayout result = m_slide_layout;
  if (const auto it = m_slide_backgrounds.find(element_id);
      it != std::end(m_slide_backgrounds)) {
    result.background_color = it->second;
  }
  return result;
}

const ElementRegistry &Document::element_registry() const {
  return m_element_registry;
}

namespace {

using TreeEditor = xml::TreeEditor<ElementRegistry>;

/// [ECMA-376] 19.7.10: the placeholders a slide types into; a graphic one
/// (`pic`, `chart`, `tbl`, ...) and the date, footer and number are left out.
bool takes_text(const pugi::xml_node placeholder) {
  const pugi::xml_attribute type = placeholder.attribute("type");
  if (!type) {
    return true;
  }
  constexpr std::array<std::string_view, 5> text_types{
      "title", "ctrTitle", "subTitle", "body", "obj"};
  return std::ranges::find(text_types, std::string_view(type.value())) !=
         text_types.end();
}

} // namespace

void write_placeholder_slide(pugi::xml_document &slide,
                             const pugi::xml_document &layout) {
  pugi::xml_node root = slide.append_child("p:sld");
  for (const pugi::xml_attribute attribute :
       layout.document_element().attributes()) {
    if (std::string_view(attribute.name()).starts_with("xmlns")) {
      root.append_attribute(attribute.name()).set_value(attribute.value());
    }
  }
  pugi::xml_node tree = root.append_child("p:cSld").append_child("p:spTree");
  pugi::xml_node group = tree.append_child("p:nvGrpSpPr");
  pugi::xml_node group_properties = group.append_child("p:cNvPr");
  group_properties.append_attribute("id").set_value(1);
  group_properties.append_attribute("name").set_value("");
  group.append_child("p:cNvGrpSpPr");
  group.append_child("p:nvPr");
  tree.append_child("p:grpSpPr");

  std::uint32_t shape_id = 2;
  for (const pugi::xml_node shape : layout.document_element()
                                        .child("p:cSld")
                                        .child("p:spTree")
                                        .children("p:sp")) {
    const pugi::xml_node placeholder =
        shape.child("p:nvSpPr").child("p:nvPr").child("p:ph");
    if (!placeholder || !takes_text(placeholder)) {
      continue;
    }
    // the position and the text style come from the layout
    pugi::xml_node copy = tree.append_child("p:sp");
    pugi::xml_node non_visual = copy.append_child("p:nvSpPr");
    pugi::xml_node properties = non_visual.append_child("p:cNvPr");
    properties.append_attribute("id").set_value(shape_id++);
    properties.append_attribute("name").set_value(
        shape.child("p:nvSpPr").child("p:cNvPr").attribute("name").value());
    non_visual.append_child("p:cNvSpPr")
        .append_child("a:spLocks")
        .append_attribute("noGrp")
        .set_value("1");
    pugi::xml_node ph = non_visual.append_child("p:nvPr").append_child("p:ph");
    for (const char *name : {"type", "orient", "sz", "idx"}) {
      if (const pugi::xml_attribute attribute = placeholder.attribute(name)) {
        ph.append_attribute(name).set_value(attribute.value());
      }
    }
    copy.append_child("p:spPr");
    pugi::xml_node body = copy.append_child("p:txBody");
    body.append_child("a:bodyPr");
    body.append_child("a:lstStyle");
    body.append_child("a:p");
  }

  root.append_child("p:clrMapOvr").append_child("a:masterClrMapping");
}

namespace {

/// [ECMA-376] 21.1.2.3.9 `CT_TextCharacterProperties`: the children are a
/// sequence, one fill among them.
constexpr std::array<std::string_view, 22> run_property_order{
    "a:ln",        "a:noFill",    "a:solidFill",  "a:gradFill",
    "a:blipFill",  "a:pattFill",  "a:grpFill",    "a:effectLst",
    "a:effectDag", "a:highlight", "a:uLnTx",      "a:uLn",
    "a:uFillTx",   "a:uFill",     "a:latin",      "a:ea",
    "a:cs",        "a:sym",       "a:hlinkClick", "a:hlinkMouseOver",
    "a:rtl",       "a:extLst"};

constexpr std::array<const char *, 6> fill_names{"a:noFill",   "a:solidFill",
                                                 "a:gradFill", "a:blipFill",
                                                 "a:pattFill", "a:grpFill"};

/// Writes the set fields of @p style into an `a:rPr`: the toggles and the
/// size as attributes, the colour and the highlight as children.
void write_run_properties(pugi::xml_node properties, const TextStyle &style) {
  const auto attribute = [&](const char *name, const std::string &value) {
    xml::set_attribute(properties, name, value.c_str());
  };
  const auto solid = [&](const char *name, const Color &color) {
    xml::insert_in_sequence(properties, name, run_property_order)
        .append_child("a:srgbClr")
        .append_attribute("val")
        .set_value(hex_color(color).c_str());
  };

  if (style.font_weight.has_value()) {
    attribute("b", *style.font_weight == FontWeight::bold ? "1" : "0");
  }
  if (style.font_style.has_value()) {
    attribute("i", *style.font_style == FontStyle::italic ? "1" : "0");
  }
  if (style.font_underline.has_value()) {
    attribute("u", *style.font_underline ? "sng" : "none");
  }
  if (style.font_line_through.has_value()) {
    attribute("strike", *style.font_line_through ? "sngStrike" : "noStrike");
  }
  if (style.font_size.has_value()) {
    attribute("sz",
              std::to_string(std::lround(points(*style.font_size) * 100.0)));
  }
  if (style.font_color.has_value()) {
    for (const char *name : fill_names) {
      properties.remove_child(name);
    }
    solid("a:solidFill", *style.font_color);
  }
  if (style.background_color.has_value()) {
    // there is no `none`: an absent highlight is none
    properties.remove_child("a:highlight");
    if (style.background_color->alpha != 0) {
      solid("a:highlight", *style.background_color);
    }
  }
}
using xml::NodeSpan;

using AdapterBase = internal::RegistryElementAdapter<
    ElementRegistry, abstract::SlideAdapter, abstract::LineBreakAdapter,
    abstract::ParagraphAdapter, abstract::SpanAdapter, abstract::TextAdapter,
    abstract::LinkAdapter, abstract::BookmarkAdapter, abstract::TableAdapter,
    abstract::TableColumnAdapter, abstract::TableRowAdapter,
    abstract::TableCellAdapter, abstract::FrameAdapter, abstract::ImageAdapter>;

class ElementAdapter final : public AdapterBase {
public:
  ElementAdapter(Document &document, ElementRegistry &registry)
      : AdapterBase(registry), m_document(&document) {}

  [[nodiscard]] bool element_is_editable(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return true;
  }

  [[nodiscard]] std::vector<std::string> element_slide_layouts(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    std::vector<std::string> result;
    for (auto &[name, _] : m_document->slide_layouts()) {
      result.push_back(std::move(name));
    }
    return result;
  }
  [[nodiscard]] ElementIdentifier
  element_insert_slide([[maybe_unused]] const ElementIdentifier element_id,
                       const std::uint32_t index,
                       const std::uint32_t layout) const override {
    return m_document->insert_slide(index, layout);
  }

  [[nodiscard]] PageLayout
  slide_page_layout(const ElementIdentifier element_id) const override {
    return m_document->slide_page_layout(element_id);
  }
  /// TODO the slide master is not parsed into the tree, so a slide reports
  /// none. What the master paints is missing from the render with it.
  [[nodiscard]] ElementIdentifier slide_master_page(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }
  [[nodiscard]] std::string
  slide_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).child("p:cSld").attribute("name").value();
  }

  [[nodiscard]] TextStyle
  line_break_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] ParagraphStyle
  paragraph_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).paragraph_style;
  }
  [[nodiscard]] TextStyle
  paragraph_text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }
  /// [ECMA-376] 20.1.10.59 `ST_TextAlignType` names no `start` and no `end`,
  /// so those refuse.
  void paragraph_set_style(const ElementIdentifier element_id,
                           const ParagraphStyle &style) const override {
    if (!style.text_align.has_value()) {
      throw std::invalid_argument("paragraph alignment is missing");
    }
    const char *algn = nullptr;
    switch (*style.text_align) {
    case TextAlign::left:
      algn = "l";
      break;
    case TextAlign::right:
      algn = "r";
      break;
    case TextAlign::center:
      algn = "ctr";
      break;
    case TextAlign::justify:
      algn = "just";
      break;
    case TextAlign::start:
    case TextAlign::end:
      throw UnsupportedOperation();
    }
    pugi::xml_node node = get_node(element_id);
    pugi::xml_node properties = node.child("a:pPr");
    if (!properties) {
      // the schema wants it ahead of the runs
      properties = node.prepend_child("a:pPr");
    }
    xml::set_attribute(properties, "algn", algn);
  }

  [[nodiscard]] TextStyle
  span_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  [[nodiscard]] std::string
  text_content(const ElementIdentifier element_id) const override {
    const ElementRegistry::Text &text_element =
        m_registry->text_element_at(element_id);

    const pugi::xml_node first = get_node(element_id);
    const pugi::xml_node last = text_element.last;

    std::string result;
    for (pugi::xml_node node = first; node != last.next_sibling();
         node = node.next_sibling()) {
      result += get_text(node);
    }
    return result;
  }
  void text_set_content(const ElementIdentifier element_id,
                        const std::string &text) const override {
    ElementRegistry::Element &element = m_registry->element_at(element_id);
    ElementRegistry::Text &text_element =
        m_registry->text_element_at(element_id);

    const NodeSpan old_span{element.node, text_element.last};
    pugi::xml_node parent = old_span.first.parent();
    const NodeSpan new_span =
        write_text_nodes(parent, old_span.first, text, "a");

    element.node = new_span.first;
    text_element.last = new_span.last;

    xml::remove_nodes(old_span);
  }

  [[nodiscard]] ElementIdentifier
  text_insert(const ElementIdentifier element_id, const Placement where,
              const std::string &text) const override {
    const ElementIdentifier parent_id = element_parent(element_id);
    if (parent_id == null_element_id ||
        element_type(parent_id) != ElementType::span) {
      throw UnsupportedOperation();
    }
    const TreeEditor editor(*m_registry);
    const ElementIdentifier run_id = editor.isolate(element_id);
    const ElementIdentifier new_run_id = editor.insert_sibling_after(run_id);
    if (where == Placement::before) {
      pugi::xml_node parent = get_node(run_id).parent();
      parent.insert_move_before(get_node(new_run_id), get_node(run_id));
      m_registry->unlink_child(new_run_id);
      m_registry->insert_sibling_before(run_id, new_run_id);
    }
    return element_append_text(new_run_id, text);
  }

  /// Creates a missing `a:r` before `a:endParaRPr`, copying its text
  /// properties as PowerPoint does for typed text.
  [[nodiscard]] ElementIdentifier
  element_append_text(const ElementIdentifier element_id,
                      const std::string &text) const override {
    const ElementType type = element_type(element_id);
    if (type != ElementType::paragraph && type != ElementType::span) {
      throw UnsupportedOperation();
    }
    if (type == ElementType::span) {
      const ElementIdentifier child_id = element_first_child(element_id);
      if (child_id != null_element_id) {
        return text_insert(child_id, Placement::after, text);
      }
    }
    ElementIdentifier run_id = element_id;
    if (type != ElementType::span) {
      pugi::xml_node parent = get_node(element_id);
      const pugi::xml_node end = parent.child("a:endParaRPr");
      pugi::xml_node run = end ? parent.insert_child_before("a:r", end)
                               : parent.append_child("a:r");
      if (end) {
        run.append_copy(end).set_name("a:rPr");
      }
      const auto &[new_run_id, unused_run] =
          m_registry->create_element(ElementType::span, run);
      m_registry->append_child(element_id, new_run_id);
      run_id = new_run_id;
    }

    const NodeSpan span = write_text_nodes(get_node(run_id), {}, text, "a");
    const auto &[new_id, unused_element, unused_text] =
        m_registry->create_text_element(span.first, span.last);
    m_registry->append_child(run_id, new_id);
    return new_id;
  }

  void element_remove(const ElementIdentifier element_id) const override {
    if (element_type(element_id) == ElementType::slide) {
      throw UnsupportedOperation();
    }
    const ElementIdentifier parent_id = element_parent(element_id);
    if (element_type(element_id) == ElementType::text &&
        parent_id != null_element_id &&
        element_type(parent_id) == ElementType::span &&
        element_first_child(parent_id) == element_id &&
        element_next_sibling(element_id) == null_element_id) {
      TreeEditor(*m_registry).remove(parent_id);
      return;
    }
    TreeEditor(*m_registry).remove(element_id);
  }
  /// Cuts the `a:r` around the run, each part keeping the `a:rPr`, and
  /// writes into the part that holds the run.
  void text_set_style(const ElementIdentifier element_id,
                      const TextStyle &style) const override {
    const ElementIdentifier parent_id = element_parent(element_id);
    if (parent_id == null_element_id ||
        element_type(parent_id) != ElementType::span) {
      throw std::invalid_argument("the run sits in no a:r");
    }
    const ElementIdentifier run_id =
        TreeEditor(*m_registry).isolate(element_id);
    pugi::xml_node run_node = get_node(run_id);
    pugi::xml_node properties = run_node.child("a:rPr");
    if (!properties) {
      // the schema wants it ahead of the text
      properties = run_node.prepend_child("a:rPr");
    }
    write_run_properties(properties, style);
  }

  [[nodiscard]] ElementIdentifier
  paragraph_split(const ElementIdentifier element_id,
                  const ElementIdentifier after_id) const override {
    return TreeEditor(*m_registry).split(element_id, after_id);
  }

  void paragraph_merge_next(const ElementIdentifier element_id) const override {
    const ElementIdentifier next_id = element_next_sibling(element_id);
    if (next_id == null_element_id ||
        element_type(next_id) != ElementType::paragraph) {
      throw std::invalid_argument("no paragraph follows the one to merge into");
    }
    TreeEditor(*m_registry).merge_next(element_id);
  }

  [[nodiscard]] ElementIdentifier
  paragraph_insert_after(const ElementIdentifier element_id) const override {
    return TreeEditor(*m_registry).insert_sibling_after(element_id);
  }

  [[nodiscard]] TextStyle
  text_style(const ElementIdentifier element_id) const override {
    return get_intermediate_style(element_id).text_style;
  }

  /// TODO an `a:hlinkClick` relationship is not resolved, so a link in a
  /// slide has no href.
  [[nodiscard]] std::string link_href(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return {};
  }

  [[nodiscard]] std::string
  bookmark_name(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("text:name").value();
  }

  [[nodiscard]] TableDimensions
  table_dimensions(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);

    TableDimensions result;
    result.columns = static_cast<std::uint32_t>(
        std::ranges::distance(node.child("a:tblGrid").children("a:gridCol")));
    result.rows = static_cast<std::uint32_t>(
        std::ranges::distance(node.children("a:tr")));
    return result;
  }
  [[nodiscard]] ElementIdentifier
  table_first_column(const ElementIdentifier element_id) const override {
    return m_registry->table_element_at(element_id).first_column_id;
  }
  [[nodiscard]] ElementIdentifier
  table_first_row(const ElementIdentifier element_id) const override {
    return element_first_child(element_id);
  }
  [[nodiscard]] TableStyle
  table_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_style;
  }

  [[nodiscard]] TableColumnStyle
  table_column_style(const ElementIdentifier element_id) const override {
    TableColumnStyle result;
    if (const std::optional<Measure> width =
            read_emus_attribute(get_node(element_id).attribute("w"));
        width.has_value()) {
      result.width = width;
    }
    return result;
  }

  [[nodiscard]] TableRowStyle
  table_row_style(const ElementIdentifier element_id) const override {
    TableRowStyle result;
    if (const std::optional<Measure> height =
            read_emus_attribute(get_node(element_id).attribute("h"));
        height.has_value()) {
      result.height = height;
    }
    return result;
  }

  [[nodiscard]] bool
  table_cell_is_covered(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return node.attribute("hMerge").as_bool() ||
           node.attribute("vMerge").as_bool();
  }
  [[nodiscard]] TableDimensions
  table_cell_span(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    return {node.attribute("rowSpan").as_uint(1),
            node.attribute("gridSpan").as_uint(1)};
  }
  [[nodiscard]] ValueType table_cell_value_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return ValueType::string;
  }
  [[nodiscard]] TableCellStyle
  table_cell_style(const ElementIdentifier element_id) const override {
    return get_partial_style(element_id).table_cell_style;
  }

  [[nodiscard]] AnchorType frame_anchor_type(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return AnchorType::at_page;
  }
  [[nodiscard]] std::optional<Measure>
  frame_x(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:off").attribute("x"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_y(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:off").attribute("y"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_width(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:ext").attribute("cx"));
  }
  [[nodiscard]] std::optional<Measure>
  frame_height(const ElementIdentifier element_id) const override {
    return read_emus_attribute(
        get_frame_xfrm(element_id).child("a:ext").attribute("cy"));
  }
  [[nodiscard]] std::optional<std::int32_t> frame_z_index(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    return std::nullopt;
  }
  [[nodiscard]] std::optional<DrawingTransform> frame_transform(
      [[maybe_unused]] const ElementIdentifier element_id) const override {
    // TODO the rotation `a:xfrm` carries
    return std::nullopt;
  }
  [[nodiscard]] GraphicStyle
  frame_style(const ElementIdentifier element_id) const override {
    const pugi::xml_node node = get_node(element_id);
    const pugi::xml_node shape_properties = node.child("p:spPr");

    GraphicStyle result;
    if (shape_properties.child("a:noFill")) {
      result.fill_color = Color(0, 0, 0, 0);
    } else if (const std::optional<Color> fill_color =
                   read_drawing_color(shape_properties.child("a:solidFill"),
                                      get_color_scheme(element_id));
               fill_color.has_value()) {
      result.fill_color = fill_color;
    }
    const PlaceholderSources sources = get_placeholder_sources(element_id);
    for (const pugi::xml_node shape :
         {sources.master_shape, sources.layout_shape, node}) {
      if (const std::optional<VerticalAlign> vertical_align =
              read_drawing_vertical_align_attribute(shape.child("p:txBody")
                                                        .child("a:bodyPr")
                                                        .attribute("anchor"));
          vertical_align.has_value()) {
        result.vertical_align = vertical_align;
      }
    }
    return result;
  }

  [[nodiscard]] bool
  image_is_internal(const ElementIdentifier element_id) const override {
    try {
      const AbsPath path = Path(image_href(element_id)).make_absolute();
      return m_document->as_filesystem()->is_file(path);
    } catch (...) { // NOLINT(bugprone-empty-catch): any error => not internal
    }
    return false;
  }
  [[nodiscard]] std::optional<File>
  image_file(const ElementIdentifier element_id) const override {
    const AbsPath path = Path(image_href(element_id)).make_absolute();
    return File(m_document->as_filesystem()->open(path));
  }
  [[nodiscard]] std::string
  image_href(const ElementIdentifier element_id) const override {
    return get_node(element_id).attribute("xlink:href").value();
  }

private:
  Document *m_document{nullptr};

  [[nodiscard]] pugi::xml_node
  get_node(const ElementIdentifier element_id) const {
    return m_registry->element_at(element_id).node;
  }

  /// The layout, master and theme of the slide the element sits on.
  [[nodiscard]] const LayoutStyle *
  get_layout_style(const ElementIdentifier element_id) const {
    for (ElementIdentifier id = element_id; id != null_element_id;
         id = element_parent(id)) {
      if (element_type(id) == ElementType::slide) {
        return m_document->slide_style(id);
      }
    }
    return nullptr;
  }

  [[nodiscard]] const Theme *
  get_theme(const ElementIdentifier element_id) const {
    const LayoutStyle *style = get_layout_style(element_id);
    return style == nullptr ? nullptr : style->theme;
  }

  [[nodiscard]] const ColorScheme *
  get_color_scheme(const ElementIdentifier element_id) const {
    const Theme *theme = get_theme(element_id);
    return theme == nullptr ? nullptr : &theme->color_scheme;
  }

  /// The placeholders of the layout and master the frame @p element_id
  /// inherits from; none for a shape that is no placeholder.
  [[nodiscard]] PlaceholderSources
  get_placeholder_sources(const ElementIdentifier element_id) const {
    const LayoutStyle *style = get_layout_style(element_id);
    if (style == nullptr) {
      return {};
    }
    return find_placeholder_sources(get_node(element_id), style->layout,
                                    style->master);
  }

  /// `p:sp` carries its transform in `p:spPr/a:xfrm`, `p:graphicFrame` in
  /// `p:xfrm`. A placeholder that states none takes its layout's, else its
  /// master's.
  [[nodiscard]] pugi::xml_node
  get_frame_xfrm(const ElementIdentifier element_id) const {
    const pugi::xml_node node = get_node(element_id);
    if (const pugi::xml_node xfrm = node.child("p:spPr").child("a:xfrm")) {
      return xfrm;
    }
    if (const pugi::xml_node xfrm = node.child("p:xfrm")) {
      return xfrm;
    }
    const PlaceholderSources sources = get_placeholder_sources(element_id);
    if (const pugi::xml_node xfrm =
            sources.layout_shape.child("p:spPr").child("a:xfrm")) {
      return xfrm;
    }
    return sources.master_shape.child("p:spPr").child("a:xfrm");
  }

  /// The list styles a paragraph inherits, weakest first: the presentation's
  /// default, the master's title or body style, then the `a:lstStyle` of the
  /// master's, the layout's and the shape's own text body. [ECMA-376] 19.3.1.36
  [[nodiscard]] std::array<pugi::xml_node, 5>
  get_list_styles(const ElementIdentifier paragraph_id) const {
    pugi::xml_node text_body;
    ElementIdentifier frame_id = null_element_id;
    for (ElementIdentifier id = element_parent(paragraph_id);
         id != null_element_id; id = element_parent(id)) {
      const ElementType type = element_type(id);
      if (!text_body && type == ElementType::group) {
        text_body = get_node(id);
      }
      if (type == ElementType::frame) {
        frame_id = id;
        break;
      }
    }

    const PlaceholderSources sources = frame_id == null_element_id
                                           ? PlaceholderSources{}
                                           : get_placeholder_sources(frame_id);
    return {m_document->default_text_style(), sources.master_list_style,
            sources.master_shape.child("p:txBody").child("a:lstStyle"),
            sources.layout_shape.child("p:txBody").child("a:lstStyle"),
            text_body.child("a:lstStyle")};
  }

  [[nodiscard]] static std::string get_text(const pugi::xml_node node) {
    const std::string name = node.name();

    if (name == "a:t") {
      return node.text().get();
    }
    if (name == "a:tab") {
      return "\t";
    }

    return "";
  }

  [[nodiscard]] ResolvedStyle
  get_partial_style(const ElementIdentifier element_id) const {
    const ElementRegistry::Element &element =
        m_registry->element_at(element_id);
    if (element.type == ElementType::paragraph) {
      ResolvedStyle result;
      const Theme *theme = get_theme(element_id);
      const pugi::xml_node own = element.node.child("a:pPr");
      // [ECMA-376] 21.1.2.2.7: `lvl` 0 to 8 picks `a:lvl1pPr` to `a:lvl9pPr`
      const std::string level = fmt::format(
          "a:lvl{}pPr", std::clamp(own.attribute("lvl").as_int(0), 0, 8) + 1);
      for (const pugi::xml_node list_style : get_list_styles(element_id)) {
        const pugi::xml_node properties = list_style.child(level.c_str());
        resolve_paragraph_properties(properties, result.paragraph_style);
        resolve_text_properties(properties.child("a:defRPr"), theme,
                                result.text_style);
      }
      resolve_paragraph_properties(own, result.paragraph_style);
      return result;
    }
    if (element.type == ElementType::span) {
      ResolvedStyle result;
      resolve_text_properties(element.node.child("a:rPr"),
                              get_theme(element_id), result.text_style);
      return result;
    }
    return {};
  }

  [[nodiscard]] ResolvedStyle
  get_intermediate_style(const ElementIdentifier element_id) const {
    const ElementIdentifier parent_id = element_parent(element_id);
    if (parent_id == null_element_id) {
      return get_partial_style(element_id);
    }
    ResolvedStyle base = get_intermediate_style(parent_id);
    base.override(get_partial_style(element_id));
    return base;
  }
};

std::unique_ptr<abstract::ElementAdapter>
create_element_adapter(Document &document, ElementRegistry &registry) {
  return std::make_unique<ElementAdapter>(document, registry);
}

} // namespace

} // namespace odr::internal::ooxml::presentation
