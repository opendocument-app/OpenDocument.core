#include <cstring>
#include <glog/logging.h>
#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/crypto/crypto_util.h>
#include <internal/ooxml/ooxml_presentation_translator.h>
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
void xfrm_translator(const pugi::xml_node &in, std::ostream &out, Context &) {
  if (const auto off_ele = in.child("a:off"); off_ele) {
    const float x_in = off_ele.attribute("x").as_float() / 914400.0f;
    const float y_in = off_ele.attribute("y").as_float() / 914400.0f;
    out << "position:absolute;";
    out << "left:" << x_in << "in;";
    out << "top:" << y_in << "in;";
  }

  if (const auto ext_ele = in.child("a:ext"); ext_ele) {
    const float cx_in = ext_ele.attribute("cx").as_float() / 914400.0f;
    const float cy_in = ext_ele.attribute("cy").as_float() / 914400.0f;
    out << "width:" << cx_in << "in;";
    out << "height:" << cy_in << "in;";
  }
}

void border_translator(const std::string &property, const pugi::xml_node &in,
                       std::ostream &out, Context &) {
  const auto w_attr = in.attribute("w");
  if (!w_attr) {
    return;
  }

  const auto color_ele = in.child("a:solidFill").child("a:srgbClr");
  if (!color_ele) {
    return;
  }

  const auto val_attr = color_ele.attribute("val");
  if (!val_attr) {
    return;
  }

  out << property << ":" << w_attr.as_float() / 14400.0f << "pt solid ";
  if (std::strlen(val_attr.as_string()) == 6) {
    out << "#";
  }
  out << val_attr.as_string();
  out << ";";
}

void background_color_translator(const pugi::xml_node &in, std::ostream &out,
                                 Context &) {
  const auto color_ele = in.child("a:srgbClr");
  if (!color_ele) {
    return;
  }

  const auto val_attr = color_ele.attribute("val");
  if (!val_attr) {
    return;
  }

  out << "background-color:";
  if (std::strlen(val_attr.as_string()) == 6) {
    out << "#";
  }
  out << val_attr.as_string();
  out << ";";
}

void margin_attributes_translator(const pugi::xml_node &in, std::ostream &out,
                                  Context &) {
  const auto mar_l_attr = in.attribute("marL");
  if (mar_l_attr) {
    const float mar_l_in = mar_l_attr.as_float() / 914400.0f;
    out << "margin-left:" << mar_l_in << "in;";
  }

  const auto mar_r_attr = in.attribute("marR");
  if (mar_r_attr) {
    const float mar_r_in = mar_r_attr.as_float() / 914400.0f;
    out << "margin-right:" << mar_r_in << "in;";
  }
}

void table_cell_property_translator(const pugi::xml_node &in, std::ostream &out,
                                    Context &context) {
  margin_attributes_translator(in, out, context);

  for (auto &&e : in) {
    const std::string element = e.name();

    if (element == "a:lnL") {
      border_translator("border-left", e, out, context);
    } else if (element == "a:lnR") {
      border_translator("border-right", e, out, context);
    } else if (element == "a:lnT") {
      border_translator("border-top", e, out, context);
    } else if (element == "a:lnB") {
      border_translator("border-bottom", e, out, context);
    } else if (element == "a:solidFill") {
      background_color_translator(e, out, context);
    }
  }
}

void default_property_translator(const pugi::xml_node &in, std::ostream &out,
                                 Context &context) {
  margin_attributes_translator(in, out, context);

  const auto sz_attr = in.attribute("sz");
  if (sz_attr) {
    float szPt = sz_attr.as_float() / 100.0f;
    out << "font-size:" << szPt << "pt;";
  }

  const auto algn_attr = in.attribute("algn");
  if (algn_attr) {
    out << "text-align:";
    if (std::strcmp(algn_attr.as_string(), "l") == 0)
      out << "left";
    if (std::strcmp(algn_attr.as_string(), "ctr") == 0)
      out << "center";
    if (std::strcmp(algn_attr.as_string(), "r") == 0)
      out << "right";
    out << ";";
  }

  for (auto &&e : in) {
    if (std::strcmp(e.name(), "a:xfrm") == 0)
      xfrm_translator(e, out, context);
  }
}
} // namespace

void presentation_translator::css(const pugi::xml_node &, Context &) {}

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
    context.text_translation[context.current_text_translation_index] = in;
    ++context.current_text_translation_index;
  }
}

void style_attribute_translator(const pugi::xml_node &in, std::ostream &out,
                                Context &context) {
  const auto p_pr = in.child("a:pPr");
  const auto r_pr = in.child("a:rPr");
  const auto sp_pr = in.child("p:spPr");
  const auto tc_pr = in.child("a:tcPr");
  const auto end_para_r_pr = in.child("a:endParaRPr");
  const auto xfrm_pr = in.child("p:xfrm");
  const auto w_attr = in.attribute("w");
  const auto h_attr = in.attribute("h");
  if ((p_pr && p_pr.first_child()) || (r_pr && r_pr.first_child()) ||
      (sp_pr && sp_pr.first_child()) || (tc_pr && tc_pr.first_child()) ||
      (end_para_r_pr && end_para_r_pr.first_child()) || xfrm_pr || w_attr ||
      h_attr) {
    out << " style=\"";
    if (p_pr) {
      default_property_translator(p_pr, out, context);
    }
    if (r_pr) {
      default_property_translator(r_pr, out, context);
    }
    if (sp_pr) {
      default_property_translator(sp_pr, out, context);
    }
    if (tc_pr) {
      table_cell_property_translator(tc_pr, out, context);
    }
    if (end_para_r_pr) {
      default_property_translator(end_para_r_pr, out, context);
    }
    if (xfrm_pr) {
      xfrm_translator(xfrm_pr, out, context);
    }
    if (w_attr) {
      out << "width:" << (w_attr.as_float() / 914400.0f) << "in;";
    }
    if (h_attr) {
      out << "height:" << (h_attr.as_float() / 914400.0f) << "in;";
    }
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

void paragraph_translator(const pugi::xml_node &in, std::ostream &out,
                          Context &context) {
  out << "<p";
  element_attribute_translator(in, out, context);
  out << ">";

  const auto bu_char_ele = in.child("a:pPr").child("a:buChar");
  if (bu_char_ele) {
    // const tinyxml2::XMLAttribute *charAttr =
    // bu_char_ele->FindAttribute("char"); const tinyxml2::XMLAttribute
    // *sizeAttr
    // =
    // in.FirstChildElement("a:pPr")->FirstChildElement("a:buSzPct")->FindAttribute("val");
    // const tinyxml2::XMLAttribute *fontAttr =
    // in.FirstChildElement("a:pPr")->FirstChildElement("a:buFont")->FindAttribute("typeface");

    std::string bullet = "•";
    // TODO decide bullet, font, size on input

    out << "<span style=\"";
    // out << "font-family:" << fontAttr->Value() << " 2;font-size:" <<
    // sizeAttr->Int64Value() / 1440.0f << "pt;";
    out << "\">" << bullet << " </span>";
  }

  bool empty = true;
  for (auto &&e1 : in) {
    for (auto &&e2 : e1) {
      if (util::string::ends_with(e1.name(), "Pr")) {
      } else if (std::strcmp(e1.name(), "w:r") != 0) {
        empty = false;
      } else if (util::string::ends_with(e2.name(), "Pr")) {
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
}

void span_translator(const pugi::xml_node &in, std::ostream &out,
                     Context &context) {
  bool link = false;
  const auto hlink_click = in.child("a:rPr").child("a:hlinkClick");
  if (hlink_click && hlink_click.attribute("r:id")) {
    const auto r_id_attr = hlink_click.attribute("r:id");
    const std::string href = context.relations[r_id_attr.as_string()];
    link = true;
    out << "<a href=\"" << href << "\">";
  }

  out << "<span";
  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << "</span>";

  if (link) {
    out << "</a>";
  }
}

void slide_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  out << "<div class=\"slide\">";
  element_children_translator(in, out, context);
  out << "</div>";
}

void table_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  out << R"(<table border="0" cellspacing="0" cellpadding="0")";
  element_attribute_translator(in, out, context);
  out << ">";
  element_children_translator(in, out, context);
  out << "</table>";
}

// TODO duplicated in document translation
void image_translator(const pugi::xml_node &in, std::ostream &out,
                      Context &context) {
  out << "<img";
  element_attribute_translator(in, out, context);

  const auto ref = in.child("p:blipFill").child("a:blip");
  if (!ref || !ref.attribute("r:embed")) {
    out << " alt=\"Error: image path not specified";
    LOG(ERROR) << "image href not found";
  } else {
    const auto r_id_attr = ref.attribute("r:embed");
    const auto path = common::Path("ppt/slides")
                          .join(context.relations[r_id_attr.as_string()]);
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
      {"p:sp", "div"},
      {"p:graphicFrame", "div"},
      {"a:tblGrid", "colgroup"},
      {"a:gridCol", "col"},
      {"a:tr", "tr"},
      {"a:tc", "td"},
  };
  static std::unordered_set<std::string> skippers{};

  const std::string element = in.name();
  if (skippers.find(element) != skippers.end()) {
    return;
  }

  if (element == "a:p") {
    paragraph_translator(in, out, context);
  } else if (element == "a:r") {
    span_translator(in, out, context);
  } else if (element == "p:cSld") {
    slide_translator(in, out, context);
  } else if (element == "a:tbl") {
    table_translator(in, out, context);
  } else if (element == "p:pic") {
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

void presentation_translator::html(const pugi::xml_node &in, Context &context) {
  element_translator(in, *context.output, context);
}

} // namespace odr::internal::ooxml
