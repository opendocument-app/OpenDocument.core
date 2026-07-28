package app.opendocument.core;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * The HTTP server on a device. It is the one part of the bindings that runs the library on threads
 * cpp-httplib started rather than on a java one, which is where the android JNI implementation is
 * least forgiving (#628).
 */
@RunWith(AndroidJUnit4.class)
public class HttpServerTest {
  private Path tempDir;

  @Before
  public void setUp() throws IOException {
    TestSupport.initialize();
    tempDir = TestSupport.tempDir("http-server");
  }

  private static String fetch(String url) throws Exception {
    long deadline = System.nanoTime() + 5_000_000_000L;
    while (true) {
      HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
      connection.setConnectTimeout(1000);
      connection.setReadTimeout(1000);
      // no keep-alive connection may outlive the request: cpp-httplib's
      // listen() only returns once its workers are done
      connection.setRequestProperty("Connection", "close");
      try {
        assertEquals(200, connection.getResponseCode());
        try (InputStream body = connection.getInputStream()) {
          return read(body);
        }
      } catch (IOException e) {
        if (System.nanoTime() > deadline) {
          throw e;
        }
        Thread.sleep(50);
      } finally {
        connection.disconnect();
      }
    }
  }

  private static String read(InputStream input) throws IOException {
    ByteArrayOutputStream buffer = new ByteArrayOutputStream();
    byte[] chunk = new byte[8192];
    for (int count = input.read(chunk); count != -1; count = input.read(chunk)) {
      buffer.write(chunk, 0, count);
    }
    return new String(buffer.toByteArray(), StandardCharsets.UTF_8);
  }

  @Test
  public void serveFile() throws Exception {
    assumeTrue("built without the HTTP server", Odr.hasHttpServer());

    HttpServer server = new HttpServer();

    String cachePath = Files.createDirectories(tempDir.resolve("doc-cache")).toString();
    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlConfig htmlConfig = new HtmlConfig();
    htmlConfig.embedImages = false;
    htmlConfig.relativeResourcePaths = false;
    HtmlService service = Html.translate(file, cachePath, htmlConfig);
    server.connectService(service, "doc");
    List<HtmlView> views = service.listViews();
    assertEquals(1, views.size());

    int port = server.bind("127.0.0.1", 0);
    AtomicReference<Throwable> listenError = new AtomicReference<>();
    Thread thread =
        new Thread(
            () -> {
              try {
                server.listen();
              } catch (Throwable t) {
                listenError.set(t);
              }
            });
    thread.setDaemon(true);
    thread.start();
    try {
      String body = fetch("http://127.0.0.1:" + port + "/file/doc/" + views.get(0).path());
      assertTrue(body.contains(TestFiles.ODT_FIRST_PARAGRAPH));
    } catch (Exception e) {
      if (listenError.get() != null) {
        throw new AssertionError("listen failed", listenError.get());
      }
      throw e;
    } finally {
      server.stop();
      thread.join(5000);
    }
    assertFalse(thread.isAlive());
  }

  @Test
  public void bindReportsWhatItGot() {
    assumeTrue("built without the HTTP server", Odr.hasHttpServer());

    HttpServer server = new HttpServer();
    // a literal address on purpose: "localhost" resolves to both ::1 and
    // 127.0.0.1, so a second bind would land on the other one
    int port = server.bind("127.0.0.1", 0);
    assertNotEquals(0, port);
    server.stop();
  }
}
