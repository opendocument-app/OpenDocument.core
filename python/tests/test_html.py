from pathlib import Path

import pyodr


def translate_offline(path, tmp_path):
    file = pyodr.open(str(path))
    cache = tmp_path / "cache"
    output = tmp_path / "output"
    cache.mkdir()
    output.mkdir()
    service = pyodr.html.translate(file, str(cache), pyodr.HtmlConfig())
    return service.bring_offline(str(output))


def test_html_config_defaults():
    config = pyodr.HtmlConfig()
    assert config.embed_images
    assert not config.editable
    assert config.spreadsheet_gridlines == pyodr.HtmlTableGridlines.soft

    config.editable = True
    config.format_html = True
    config.spreadsheet_limit = pyodr.TableDimensions(100, 100)
    assert config.editable
    assert config.spreadsheet_limit.rows == 100


def test_html_config_viewport_defaults():
    config = pyodr.HtmlConfig()
    assert config.viewport_mode == pyodr.HtmlViewportMode.automatic
    assert config.spreadsheet_viewport_mode is None
    assert config.viewport_content is None

    config.viewport_mode = pyodr.HtmlViewportMode.fit_width
    config.spreadsheet_viewport_mode = pyodr.HtmlViewportMode.actual_size
    config.viewport_content = "width=420"
    assert config.viewport_mode == pyodr.HtmlViewportMode.fit_width
    assert config.spreadsheet_viewport_mode == pyodr.HtmlViewportMode.actual_size
    assert config.viewport_content == "width=420"


def test_viewport_mode_reaches_the_html(core_data_path, odt_path, tmp_path):
    # The C++ suite covers the mode matrix; this only proves the config crosses
    # the binding. A text document without margins is reflowing content, so
    # `automatic` resolves to `actual_size`.
    def render(name, config):
        cache = tmp_path / name
        cache.mkdir()
        file = pyodr.open(str(odt_path))
        service = pyodr.html.translate(file, str(cache), config)
        content, _ = service.list_views()[0].write_html()
        return content

    assert (
        '<meta name="viewport" '
        'content="width=device-width,initial-scale=1.0,user-scalable=yes"/>'
        in render("automatic", pyodr.HtmlConfig())
    )

    fit_width = pyodr.HtmlConfig()
    fit_width.viewport_mode = pyodr.HtmlViewportMode.fit_width
    assert (
        '<meta name="viewport" content="width=device-width,user-scalable=yes"/>'
        in render("fit_width", fit_width)
    )

    raw = pyodr.HtmlConfig()
    raw.viewport_content = "width=420"
    assert '<meta name="viewport" content="width=420"/>' in render("raw", raw)


def test_translate_text(core_data_path, txt_path, tmp_path):
    html = translate_offline(txt_path, tmp_path)
    pages = html.pages()
    assert len(pages) == 1
    content = Path(pages[0].path).read_text()
    assert "hello text file" in content


def test_translate_csv(core_data_path, csv_path, tmp_path):
    html = translate_offline(csv_path, tmp_path)
    pages = html.pages()
    assert len(pages) == 1
    content = Path(pages[0].path).read_text()
    assert "alpha" in content


def test_translate_document(core_data_path, odt_path, tmp_path):
    html = translate_offline(odt_path, tmp_path)
    pages = html.pages()
    assert len(pages) == 1
    content = Path(pages[0].path).read_text()
    assert "Hello from pyodr!" in content


def test_html_service_views(core_data_path, odt_path, tmp_path):
    file = pyodr.open(str(odt_path))
    cache = tmp_path / "cache"
    cache.mkdir()
    service = pyodr.html.translate(file, str(cache), pyodr.HtmlConfig())

    views = service.list_views()
    assert len(views) == 1

    content, resources = views[0].write_html()
    assert "Hello from pyodr!" in content
    assert isinstance(resources, list)


def test_html_view_outlives_service(core_data_path, odt_path, tmp_path):
    file = pyodr.open(str(odt_path))
    cache = tmp_path / "cache"
    cache.mkdir()

    # The service temporary is dropped immediately; the view must keep it
    # alive.
    view = pyodr.html.translate(file, str(cache), pyodr.HtmlConfig()).list_views()[0]

    content, _ = view.write_html()
    assert "Hello from pyodr!" in content
