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


def _init_odr_core_data_path() -> None:
    # Rendering needs the shipped odr.js/css assets. Resolution order: an
    # already-configured valid path wins, then the ODR_CORE_DATA_PATH
    # environment variable, then the assets bundled with the package. The
    # built-in default is an install-relative guess ("share"), so only an
    # existing directory counts as configured.
    current = _core.GlobalParams.odr_core_data_path()
    if current and os.path.isdir(current):
        return
    env_path = os.environ.get("ODR_CORE_DATA_PATH")
    if env_path:
        _core.GlobalParams.set_odr_core_data_path(env_path)
        return
    if _BUNDLED_DATA_PATH.is_dir():
        _core.GlobalParams.set_odr_core_data_path(str(_BUNDLED_DATA_PATH))


def _init_libmagic_database_path() -> None:
    # Same resolution order for libmagic's compiled database, which is a file
    # rather than a directory. Leaving it unresolved is not fatal: odrcore then
    # tries the system database and finally falls back to its own sniffing.
    current = _core.GlobalParams.libmagic_database_path()
    if current and os.path.isfile(current):
        return
    env_path = os.environ.get("ODR_LIBMAGIC_DATABASE_PATH")
    if env_path:
        _core.GlobalParams.set_libmagic_database_path(env_path)
        return
    database_path = _BUNDLED_DATA_PATH / "magic.mgc"
    if database_path.is_file():
        _core.GlobalParams.set_libmagic_database_path(str(database_path))


def _init_data_paths() -> None:
    _init_odr_core_data_path()
    _init_libmagic_database_path()


_init_data_paths()
