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
