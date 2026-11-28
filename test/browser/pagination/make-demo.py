#!/usr/bin/env python3
"""Render the interactive example with the `pyodr` on `PYTHONPATH`."""
from pathlib import Path
from xml.sax.saxutils import escape

import pyodr

HERE = Path(__file__).resolve().parent

passages = [
    "The long paragraph begins here. At sunrise we followed the river out of town. The path ran beside old warehouses, then narrowed into a trail between the water and the trees. Every few minutes the light changed: first silver on the surface, then gold along the reeds. We stopped on a wooden bridge to listen to the sound beneath it.",
    "Beyond the bridge, the river made a slow turn around a meadow. A cyclist passed with a basket full of bread, and a dog watched us from the opposite bank. There was no particular destination for the morning. We wanted to see how far the path would go and whether the map had remembered all the small crossings.",
    "A notebook is useful on a walk like this. It gives small observations somewhere to stay: the name on a boat, the distance between two bridges, the color of a door reflected in the water. None of these details needs a page to itself. They belong to the same account, even when the account becomes too long for a single sheet.",
    "By midday we reached a quiet garden behind the old mill. Someone had planted rosemary beside the gate, and its scent followed us along the wall. We sat under a tree and compared our notes. One of us had recorded the birds; the other had sketched the bridges. Together the two versions described a place neither notebook could have captured alone.",
    "The return journey felt shorter, though we took the same route. Familiar landmarks arrived in a different order. The garden came first, then the meadow, then the bridge where we had paused at sunrise. The water had risen a little, and the light now fell on the other side of the buildings. We added a few lines to the account before going home.",
    "You can select words across the page boundary in this paragraph. Choose Edit document, change a sentence directly on the page, and try Undo or Redo. The pages adjust as you type. The words still belong to this one paragraph: the page boundary is a property of the view, not an extra paragraph in the document. The long paragraph ends here.",
]
source = f"""<office:document
 xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
 xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
 xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
 xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"
 office:mimetype="application/vnd.oasis.opendocument.text" office:version="1.3">
 <office:automatic-styles>
  <style:style style:name="Body" style:family="paragraph"><style:paragraph-properties fo:line-height="145%" fo:margin-bottom="8pt"/><style:text-properties fo:font-family="Georgia" fo:font-size="12pt"/></style:style>
  <style:style style:name="Title" style:family="paragraph"><style:paragraph-properties fo:margin-bottom="12pt"/><style:text-properties fo:font-family="Georgia" fo:font-size="22pt" fo:font-weight="bold"/></style:style>
  <style:style style:name="Small" style:family="paragraph"><style:text-properties fo:font-family="Arial" fo:font-size="9pt" fo:color="#64748b"/></style:style>
  <style:page-layout style:name="Paper"><style:page-layout-properties fo:page-width="14.8cm" fo:page-height="21cm" fo:margin-top="1.4cm" fo:margin-bottom="1.4cm" fo:margin-left="1.6cm" fo:margin-right="1.6cm"/><style:header-style><style:header-footer-properties fo:min-height="0.8cm" fo:margin-bottom="0.4cm"/></style:header-style><style:footer-style><style:header-footer-properties fo:min-height="0.8cm" fo:margin-top="0.4cm"/></style:footer-style></style:page-layout>
 </office:automatic-styles>
 <office:master-styles><style:master-page style:name="Standard" style:page-layout-name="Paper"><style:header><text:p text:style-name="Small">RIVER NOTEBOOK · CONTINUED</text:p></style:header><style:header-first><text:p text:style-name="Small">RIVER NOTEBOOK · A PAGINATION EXAMPLE</text:p></style:header-first><style:footer><text:p text:style-name="Small">One paragraph can continue on the next page.</text:p></style:footer></style:master-page></office:master-styles>
 <office:body><office:text><text:p text:style-name="Title">A walk beside the river</text:p><text:p text:style-name="Body">{escape(' '.join(passages))}</text:p><text:p text:style-name="Small">This short note is a separate paragraph. The entire story above is one paragraph.</text:p></office:text></office:body>
</office:document>"""
config = pyodr.HtmlConfig()
config.editable = True
# `serve` answers for the scripts from the source tree, so no rebuild is needed.
config.embed_shipped_resources = False
config.text_document_pagination = True
config.viewport_mode = pyodr.HtmlViewportMode.actual_size
service = pyodr.html.translate(
    pyodr.open(pyodr.File.from_memory(source.encode())), config
)
html, _ = service.list_views()[0].write_html()
# Keep continuous flow readable when pagination is toggled off.
html = html.replace(
    "</head>",
    "<style>.odr-text-flow{max-width:38em;margin:24px auto;padding:32px;background:white}body.odr-editing{background:#eef1f5}</style></head>",
)
(HERE / "demo-document.fodt").write_text(source)
(HERE / "demo-document.html").write_text(html)
print(HERE / "demo-document.html")
