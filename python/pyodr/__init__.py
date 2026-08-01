"""Python bindings for OpenDocument.core.

Decode office documents (ODF, OOXML, legacy MS binary, PDF, CSV, ...) and
render them to HTML.

Example:
    >>> import pyodr
    >>> file = pyodr.open("document.odt")
    >>> service = pyodr.html.translate(file, cache_path, pyodr.HtmlConfig())
    >>> html = service.bring_offline(output_path)
    >>> [page.path for page in html.pages()]
"""

from pyodr import _core
from pyodr._core import *  # noqa: F401,F403
from pyodr._core import html  # noqa: F401

__version__ = _core.version()
