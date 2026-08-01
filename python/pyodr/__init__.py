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

import os
from pathlib import Path

from pyodr import _core
from pyodr._core import *  # noqa: F401,F403
from pyodr._core import html  # noqa: F401

__version__ = _core.version()


# Runtime assets installed into the package by `python/CMakeLists.txt`.
_BUNDLED_DATA_PATH = Path(__file__).resolve().parent / "data"


def _is_configured(path: str) -> bool:
    # The compiled-in default is an install-relative guess ("share"). Resolving
    # that against the process working directory would make the outcome depend
    # on where the interpreter was launched, so only an absolute path that
    # exists counts as already configured.
    return bool(path) and os.path.isabs(path) and os.path.exists(path)


def _init_odr_core_data_path() -> None:
    # Rendering needs the shipped odr.js/css assets. Resolution order: an
    # already-configured path wins, then the ODR_CORE_DATA_PATH environment
    # variable, then the assets bundled with the package.
    if _is_configured(_core.GlobalParams.odr_core_data_path()):
        return
    env_path = os.environ.get("ODR_CORE_DATA_PATH")
    if env_path:
        _core.GlobalParams.set_odr_core_data_path(env_path)
        return
    if _BUNDLED_DATA_PATH.is_dir():
        _core.GlobalParams.set_odr_core_data_path(str(_BUNDLED_DATA_PATH))


# `GlobalParams.libmagic_database_path` is left alone: libmagic is gone, so
# there is no database to resolve and nothing that would read one.
_init_odr_core_data_path()
