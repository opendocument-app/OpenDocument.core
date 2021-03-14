#include <cstring>
#include <glog/logging.h>
#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/crypto/crypto_util.h>
#include <internal/ooxml/ooxml_document_translator.h>
#include <internal/ooxml/ooxml_translator_context.h>
#include <internal/util/stream_util.h>
#include <internal/util/string_util.h>
#include <odr/html_config.h>
#include <pugixml.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace odr::internal::ooxml {

namespace {
void alignment_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &) {
  out << "text-align:" << in.attribute("w:val").as_string() << ";";
}

void font_translator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto font_attr = in.attribute("w:cs");
  if (!font_attr) {
    return;
  }
  out << "font-family:" << font_attr.as_string() << ";";
}

void font_size_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &) {
  const auto size_attr = in.attribute("w:val");
  if (!size_attr) {
    return;
  }
  const double size = size_attr.as_int() / 2.0;
  out << "font-size:" << size << "pt;";
}

void bold_translator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto val_attr = in.attribute("w:val");
  if (val_attr) {
    return;
  }
  out << "font-weight:bold;";
}

void italic_translator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto val_attr = in.attribute("w:val");
  if (val_attr) {
    return;
  }
  out << "font-style:italic;";
}

void underline_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &) {
  const auto val_attr = in.attribute("w:val");
  if (std::strcmp(val_attr.as_string(), "single") == 0) {
    out << "text-decoration:underline;";
  }
  // TODO wont work with strike_through_translator
}

void strike_through_translator(const pugi::xml_node &in, std::ostream &out,
                               Context &) {
  // TODO wont work with underline_translator

  const auto val_attr = in.attribute("w:val");
  if (val_attr && (std::strcmp(val_attr.as_string(), "false") == 0)) {
    return;
  }
  out << "text-decoration:line-through;";
}

void shadow_translator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "text-shadow:1pt 1pt;";
}

void color_translator(const pugi::xml_node &in, std::ostream &out, Context &) {
  const auto val_attr = in.attribute("w:val");
  if (std::strcmp(val_attr.as_string(), "auto") == 0) {
    return;
  }
  if (std::strlen(val_attr.as_string()) == 6) {
    out << "color:#" << val_attr.as_string() << ";";
  } else {
    out << "color:" << val_attr.as_string() << ";";
  }
}

void highlight_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &) {
  const auto val_attr = in.attribute("w:val");
  if (std::strcmp(val_attr.as_string(), "auto") == 0) {
    return;
  }
  if (std::strlen(val_attr.as_string()) == 6) {
    out << "background-color:#" << val_attr.as_string() << ";";
  } else {
    out << "background-color:" << val_attr.as_string() << ";";
  }
}

void indentation_translator(const pugi::xml_node &in, std::ostream &out,
                            Context &) {
  const auto left_attr = in.attribute("w:left");
  if (left_attr) {
    out << "margin-left:" << left_attr.as_float() / 1440.0f << "in;";
  }

  const auto right_attr = in.attribute("w:right");
  if (right_attr) {
    out << "margin-right:" << right_attr.as_float() / 1440.0f << "in;";
  }
}

void table_cell_width_translator(const pugi::xml_node &in, std::ostream &out,
                                 Context &) {
  const auto width_attr = in.attribute("w:w");
  const auto type_attr = in.attribute("w:type");
  if (!width_attr) {
    return;
  }
  float width = width_attr.as_float();
  if (type_attr && std::strcmp(type_attr.as_string(), "dxa") == 0) {
    width /= 1440.0f;
  }
  out << "width:" << width << "in;";
}

void table_cell_border_translator(const pugi::xml_node &in, std::ostream &out,
                                  Context &) {
  auto translator = [&](const char *name, const pugi::xml_node &e) {
    out << name << ":";

    const auto val = e.attribute("w:val").as_string();
    if (std::strcmp(val, "nil") == 0) {
      out << "none";
    } else {
      const float sizePt = e.attribute("w:sz").as_float() / 2.0f;
      out << sizePt << "pt ";

      out << "solid ";

      const auto colorAttr = e.attribute("w:color");
      if (std::strlen(colorAttr.as_string()) == 6) {
        out << "#" << colorAttr.as_string();
      } else {
        out << colorAttr.as_string();
      }
    }

    out << ";";
  };

  if (const auto top = in.child("w:top"); top) {
    translator("border-top", top);
  }

  if (const auto top = in.child("w:left"); top) {
    translator("border-left", top);
  }

  if (const auto top = in.child("w:bottom"); top) {
    translator("border-bottom", top);
  }

  if (const auto top = in.child("w:right"); top) {
    translator("border-right", top);
  }
}

void translate_style_inline(const pugi::xml_node &in, std::ostream &out,
                            Context &context) {
  for (auto &&e : in.children()) {
    const std::string element = e.name();

    if (element == "w:jc") {
      alignment_translator(e, out, context);
    } else if (element == "w:rFonts") {
      font_translator(e, out, context);
    } else if (element == "w:sz") {
      font_size_translator(e, out, context);
    } else if (element == "w:b") {
      bold_translator(e, out, context);
    } else if (element == "w:i") {
      italic_translator(e, out, context);
    } else if (element == "w:u") {
      underline_translator(e, out, context);
    } else if (element == "w:strike") {
      strike_through_translator(e, out, context);
    } else if (element == "w:shadow") {
      shadow_translator(e, out, context);
    } else if (element == "w:color") {
      color_translator(e, out, context);
    } else if (element == "w:highlight") {
      highlight_translator(e, out, context);
    } else if (element == "w:ind") {
      indentation_translator(e, out, context);
    } else if (element == "w:tcW") {
      table_cell_width_translator(e, out, context);
    } else if (element == "w:tcBorders") {
      table_cell_border_translator(e, out, context);
    }
  }
}

void style_class_translator(const pugi::xml_node &in, std::ostream &out,
                            Context &context) {
  std::string name = "unknown";
  if (const auto name_attr = in.attribute("w:styleId"); name_attr) {
    name = name_attr.value();
  } else {
    LOG(WARNING) << "no name attribute " << in.name();
  }

  std::string type = "unknown";
  if (const auto type_attr = in.attribute("w:type"); type_attr) {
    type = type_attr.value();
  } else {
    LOG(WARNING) << "no type attribute " << in.name();
  }

  /*
  const char *parentStyleName;
  if (in.QueryStringAttribute("style:parent-style-name", &parentStyleName) ==
  tinyxml2::XML_SUCCESS) {
      context.odStyleDependencies[styleName].push_back(parentStyleName);
  }
   */

  out << "." << name << " {";

  if (const auto p_pr = in.child("w:p_pr"); p_pr) {
    translate_style_inline(p_pr, out, context);
  }

  if (const auto r_pr = in.child("w:r_pr"); r_pr) {
    translate_style_inline(r_pr, out, context);
  }

  out << "}\n";
}
} // namespace

void document_translator::css(const pugi::xml_node &in, Context &context) {
  for (auto &&e : in.children()) {
    const std::string element = e.name();

    if (element == "w:style") {
      style_class_translator(e, *context.output, context);
    }
    // else if (element == "w:docDefaults") DefaultStyleTranslator(e,
    // *context.output, context);
  }
}

namespace {
void text_translator(const pugi::xml_text &in, std::ostream &out,
                     Context &context) {
  std::string text = in.as_string();
  util::string::replace_all(text, "&", "&amp;");
  util::string::replace_all(text, "<", "&lt;");
  util::string::replace_all(text, ">", "&gt;");

  if (!context.config->editable) {
    out << text;
  } else {
    out << R"(<span contenteditable="true" data-odr-cid=")"
        << context.current_text_translation_index << "\">" << text << "</span>";
    context.text_translation[context.current_text_translation_index] = &in;
    ++context.current_text_translation_index;
  }
}

void style_attribute_translator(const pugi::xml_node &in, std::ostream &out,
                                Context &context) {
  const std::string prefix = in.name();

  const pugi::xml_node style =
      in.child((prefix + "Pr").c_str()).child((prefix + "Style").c_str());
  if (style) {
    *context.output << " class=\"" << style.attribute("w:val").as_string()
                    << "\"";
  }

  const pugi::xml_node inline_style = in.child((prefix + "Pr").c_str());
  if (inline_style && inline_style.first_child()) {
    out << " style=\"";
    translate_style_inline(inline_style, out, context);
    out << "\"";
  }
}

void element_attribute_translator(const pugi::xml_node &in, std::ostream &out,
                                  Context &context) {
  style_attribute_translator(in, out, context);
}

void element_children_translator(const pugi::xml_node &in, std::ostream &out,
                                 Context &context);
void element_translator(const pugi::xml_node &in, std::ostream &out,
                        Context &context);

void tab_translator(const pugi::xml_node &, std::ostream &out, Context &) {
  out << "\t";
}

void paragraph_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  const pugi::xml_node num = in.child("w:pPr").child("w:numPr");
  int listing_level;
  if (num) {
    listing_level = num.child("w:ilvl").attribute("w:val").as_int();
    for (int i = 0; i <= listing_level; ++i) {
      out << "<ul>";
    }
    out << "<li>";
  }

  out << "<p";
  element_attribute_translator(in, out, context);
  out << ">";

  bool empty = true;
  for (auto &&e1 : in) {
    for (auto &&e2 : e1) {
      if (util::string::ends_with(e1.name(), "Pr")) {
      } else if (std::strcmp(e1.name(), "w:r") != 0) {
        empty = false;
      } else if (!util::string::ends_with(e2.name(), "Pr")) {
        empty = false;
      }
    }
  }

  if (empty) {
    out << "<br/>";
  } else {
    element_children_translator(in, out, context);
  }

  out << "</p>";

  if (num) {
    out << "</li>";
    for (int i = 0; i <= listing_level; ++i) {
      out << "</ul>";
    }
  }
}

void span_translator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  out << "<span";
  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << "</span>";
}

void hyperlink_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  out << "<a";

  if (const auto anchor_attr = in.attribute("w:anchor"); anchor_attr) {
    out << " href=\"#" << anchor_attr.as_string() << R"(" target="_self")";
  } else if (const auto r_id_attr = in.attribute("r:id"); r_id_attr) {
    out << " href=\"" << context.relations[r_id_attr.as_string()] << "\"";
  }

  element_attribute_translator(in, out, context);

  out << ">";
  element_children_translator(in, out, context);
  out << "</a>";
}

void bookmark_translator(const pugi::xml_node &in, std::ostream &out,
                         Context &) {
  if (const auto name_attr = in.attribute("w:name"); name_attr) {
    out << "<a id=\"" << name_attr.as_string() << "\"/>";
  }
}

void table_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  out << R"(<table border="0" cellspacing="0" cellpadding="0")";
  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << "</table>";
}

void drawings_translator(const pugi::xml_node &in, std::ostream &out,
                         Context &context) {
  // ooxml is using amazing units
  // https://startbigthinksmall.wordpress.com/2010/01/04/points-inches-and-emus-measuring-units-in-office-open-xml/

  const auto child = in.first_child();
  if (!child) {
    return;
  }
  const auto graphic = child.child("a:graphic");
  if (!graphic) {
    return;
  }
  // TODO handle something other than inline

  out << "<div";

  if (const auto sizeEle = child.child("wp:extent"); sizeEle) {
    const float widthIn = sizeEle.attribute("cx").as_float() / 914400.0f;
    const float heightIn = sizeEle.attribute("cy").as_float() / 914400.0f;
    out << " style=\"width:" << widthIn << "in;height:" << heightIn << "in;\"";
  }

  out << ">";
  element_translator(graphic, out, context);
  out << "</div>";
}

void image_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  out << "<img style=\"width:100%;height:100%\"";

  const pugi::xml_node ref = in.child("pic:blipFill").child("a:blip");
  if (!ref || !ref.attribute("r:embed")) {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  } else {
    const char *r_id_attr = ref.attribute("r:embed").as_string();
    const auto path = common::Path("word").join(context.relations[r_id_attr]);
    out << " alt=\"Error: image not found or unsupported: " << path << "\"";
    out << " src=\"";
    std::string image =
        util::stream::read(*context.filesystem->open(path)->read());
    // hacky image/jpg working according to tom
    out << "data:image/jpg;base64, ";
    out << crypto::util::base64_encode(image);
    out << "\"";
  }

  out << "></img>";
}

void element_children_translator(const pugi::xml_node &in, std::ostream &out,
                                 Context &context) {
  for (auto &&n : in) {
    if (n.type() == pugi::node_pcdata) {
      text_translator(n.text(), out, context);
    } else if (n.type() == pugi::node_element) {
      element_translator(n, out, context);
    }
  }
}

void element_translator(const pugi::xml_node &in, std::ostream &out,
                        Context &context) {
  static std::unordered_map<std::string, const char *> substitution{
      {"w:tr", "tr"},
      {"w:tc", "td"},
  };
  static std::unordered_set<std::string> skippers{
      "w:instrText",
  };

  const std::string element = in.name();
  if (skippers.find(element) != skippers.end()) {
    return;
  }

  if (element == "w:tab") {
    tab_translator(in, out, context);
  } else if (element == "w:p") {
    paragraph_translator(in, out, context);
  } else if (element == "w:r") {
    span_translator(in, out, context);
  } else if (element == "w:hyperlink") {
    hyperlink_translator(in, out, context);
  } else if (element == "w:bookmarkStart") {
    bookmark_translator(in, out, context);
  } else if (element == "w:tbl") {
    table_translator(in, out, context);
  } else if (element == "w:drawing") {
    drawings_translator(in, out, context);
  } else if (element == "pic:pic") {
    image_translator(in, out, context);
  } else {
    const auto it = substitution.find(element);
    if (it != substitution.end()) {
      out << "<" << it->second;
      element_attribute_translator(in, out, context);
      out << ">";
    }
    element_children_translator(in, out, context);
    if (it != substitution.end()) {
      out << "</" << it->second << ">";
    }
  }
}
} // namespace

void document_translator::html(const pugi::xml_node &in, Context &context) {
  element_translator(in, *context.output, context);
}

} // namespace odr::internal::ooxml
