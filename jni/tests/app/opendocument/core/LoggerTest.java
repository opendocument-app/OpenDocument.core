package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class LoggerTest {
  @TempDir Path tempDir;

  /** A sink implemented in Java — the public extension point. */
  private static final class Collecting implements ILogger {
    final List<String> messages = Collections.synchronizedList(new ArrayList<>());
    final List<SourceLocation> locations = Collections.synchronizedList(new ArrayList<>());
    volatile int flushes = 0;
    private final LogLevel level;

    Collecting(LogLevel level) {
      this.level = level;
    }

    @Override
    public boolean willLog(LogLevel level) {
      return level.ordinal() >= this.level.ordinal();
    }

    @Override
    public void log(long epochMillis, LogLevel level, String message, SourceLocation location) {
      messages.add(message);
      locations.add(location);
    }

    @Override
    public void flush() {
      flushes++;
    }
  }

  @Test
  void nullLoggerDiscards() {
    try (Logger logger = Logger.nullLogger()) {
      assertFalse(logger.willLog(LogLevel.FATAL));
    }
  }

  @Test
  void customSinkReceivesMessages() {
    Collecting sink = new Collecting(LogLevel.WARNING);
    try (Logger logger = new Logger(sink)) {
      assertFalse(logger.willLog(LogLevel.DEBUG));
      assertTrue(logger.willLog(LogLevel.ERROR));

      logger.log(LogLevel.DEBUG, "dropped");
      logger.log(LogLevel.ERROR, "kept");
      logger.flush();
    }

    assertEquals(List.of("kept"), sink.messages);
    assertEquals(1, sink.flushes);
    assertNotNull(sink.locations.get(0).fileName);
  }

  @Test
  void customSinkIsUsableWhileOpening() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    Collecting sink = new Collecting(LogLevel.VERBOSE);
    try (Logger logger = new Logger(sink);
        DecodedFile file = Odr.open(odt.toString(), logger)) {
      assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileType());
    }
    // The sink survived the native call; whether anything was logged is up to
    // the backend, so only the fact that it did not crash is asserted here.
    assertNotNull(sink.messages);
  }

  @Test
  void stdioLoggerWorks() {
    try (Logger logger = Logger.stdio("odr-test", LogLevel.FATAL)) {
      assertFalse(logger.willLog(LogLevel.INFO));
      assertTrue(logger.willLog(LogLevel.FATAL));
    }
  }
}
