package app.opendocument.core;

/** Name and path of a rendered HTML page. Mirrors {@code odr::HtmlPage}. */
public final class HtmlPage {
  public final String name;
  public final String path;

  HtmlPage(String name, String path) {
    this.name = name;
    this.path = path;
  }

  @Override
  public String toString() {
    return "HtmlPage(name='" + name + "', path='" + path + "')";
  }
}
