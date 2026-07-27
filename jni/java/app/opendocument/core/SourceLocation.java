package app.opendocument.core;

/** Call site a log message originated from. Mirrors {@code std::source_location}. */
public final class SourceLocation {
  public final String fileName;
  public final String functionName;
  public final int line;

  SourceLocation(String fileName, String functionName, int line) {
    this.fileName = fileName;
    this.functionName = functionName;
    this.line = line;
  }

  @Override
  public String toString() {
    return fileName + ":" + line;
  }
}
