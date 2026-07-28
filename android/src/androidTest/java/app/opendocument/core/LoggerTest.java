package app.opendocument.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * A java {@link ILogger} driven from native code, on ART. The sink is reached through a global
 * reference and cached method handles, and the calls arrive on whatever thread the library works on
 * — the two places where the android JNI implementation is stricter than the JDK's (#628).
 */
@RunWith(AndroidJUnit4.class)
public class LoggerTest {
  private Path tempDir;

  @Before
  public void setUp() throws IOException {
    TestSupport.initialize();
    tempDir = TestSupport.tempDir("logger");
  }

  private static final class Collecting implements ILogger {
    final List<String> messages = Collections.synchronizedList(new ArrayList<String>());
    final List<String> threads = Collections.synchronizedList(new ArrayList<String>());
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
      threads.add(Thread.currentThread().getName());
    }

    @Override
    public void flush() {
      flushes++;
    }
  }

  @Test
  public void customSinkReceivesMessages() {
    Collecting sink = new Collecting(LogLevel.WARNING);
    try (Logger logger = new Logger(sink)) {
      assertFalse(logger.willLog(LogLevel.DEBUG));
      logger.log(LogLevel.DEBUG, "dropped");
      logger.log(LogLevel.ERROR, "kept");
      logger.flush();
    }

    assertEquals(Collections.singletonList("kept"), sink.messages);
    assertEquals(1, sink.flushes);
  }

  @Test
  public void customSinkIsUsableWhileOpening() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    Collecting sink = new Collecting(LogLevel.VERBOSE);
    try (Logger logger = new Logger(sink);
        DecodedFile file = Odr.open(odt.toString(), logger)) {
      assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileType());
    }
    assertNotNull(sink.messages);
  }

  @Test
  public void sinkIsReachedFromABackgroundThread() throws Exception {
    Collecting sink = new Collecting(LogLevel.VERBOSE);
    CountDownLatch done = new CountDownLatch(1);
    try (Logger logger = new Logger(sink)) {
      Thread thread =
          new Thread(
              () -> {
                logger.log(LogLevel.ERROR, "from a worker");
                done.countDown();
              },
              "odr-log-worker");
      thread.start();
      assertTrue(done.await(10, TimeUnit.SECONDS));
      thread.join();
    }

    assertEquals(Collections.singletonList("from a worker"), sink.messages);
    assertEquals(Collections.singletonList("odr-log-worker"), sink.threads);
  }

  @Test
  public void aThrowingSinkDoesNotDerailTheOperation() throws IOException {
    Path odt = TestFiles.odtFile(tempDir);
    ILogger sink =
        new ILogger() {
          @Override
          public boolean willLog(LogLevel level) {
            return true;
          }

          @Override
          public void log(
              long epochMillis, LogLevel level, String message, SourceLocation location) {
            throw new IllegalStateException("sink is broken");
          }

          @Override
          public void flush() {
            throw new IllegalStateException("sink is broken");
          }
        };

    // the exception is described and cleared on the native side; leaving it
    // pending would abort the next JNI call instead, which on ART kills the
    // process rather than failing the test
    try (Logger logger = new Logger(sink);
        DecodedFile file = Odr.open(odt.toString(), logger)) {
      assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileType());
    }
  }
}
