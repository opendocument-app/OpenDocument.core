package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assumptions.assumeTrue;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.ServerSocket;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class HttpServerTest {
  @TempDir Path tempDir;

  private static int freePort() throws IOException {
    try (ServerSocket socket = new ServerSocket(0)) {
      return socket.getLocalPort();
    }
  }

  private record Response(int status, String body) {}

  // HttpURLConnection with "Connection: close" so no keep-alive connection
  // outlives the request — cpp-httplib's listen() only returns after its
  // workers finish, and an open keep-alive connection delays that by the
  // keep-alive timeout, racing the thread join below.
  private static Response fetch(String url) throws Exception {
    long deadline = System.nanoTime() + Duration.ofSeconds(5).toNanos();
    while (true) {
      HttpURLConnection connection =
          (HttpURLConnection) URI.create(url).toURL().openConnection();
      connection.setConnectTimeout(1000);
      connection.setReadTimeout(1000);
      connection.setRequestProperty("Connection", "close");
      try {
        int status = connection.getResponseCode();
        try (InputStream body = connection.getInputStream()) {
          return new Response(
              status, new String(body.readAllBytes(), StandardCharsets.UTF_8));
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

  @Test
  void serveFile() throws Exception {
    assumeTrue(Odr.hasHttpServer(), "built without the HTTP server");
    assumeTrue(TestFiles.hasCoreData(), "odr core data path not available");

    HttpServer.Config config = new HttpServer.Config();
    config.cachePath = tempDir.resolve("server-cache").toString();
    HttpServer server = new HttpServer(config);

    DecodedFile file = Odr.open(TestFiles.odtFile(tempDir).toString());
    HtmlConfig htmlConfig = new HtmlConfig();
    htmlConfig.embedImages = false;
    htmlConfig.relativeResourcePaths = false;
    List<HtmlView> views = server.serveFile(file, "doc", htmlConfig);
    assertEquals(1, views.size());

    int port = freePort();
    AtomicReference<Throwable> listenError = new AtomicReference<>();
    Thread thread =
        new Thread(
            () -> {
              try {
                server.listen("127.0.0.1", port);
              } catch (Throwable t) {
                listenError.set(t);
              }
            });
    thread.setDaemon(true);
    thread.start();
    try {
      Response response =
          fetch("http://127.0.0.1:" + port + "/file/doc/" + views.get(0).path());
      assertEquals(200, response.status());
      assertTrue(response.body().contains(TestFiles.ODT_FIRST_PARAGRAPH));
    } catch (Exception e) {
      if (listenError.get() != null) {
        throw new AssertionError("listen failed", listenError.get());
      }
      throw e;
    } finally {
      server.stop();
      thread.join(Duration.ofSeconds(5).toMillis());
    }
    assertFalse(thread.isAlive());
  }
}
