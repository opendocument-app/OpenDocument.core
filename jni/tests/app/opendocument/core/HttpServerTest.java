package app.opendocument.core;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assumptions.assumeTrue;

import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.time.Duration;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class HttpServerTest {
  @TempDir Path tempDir;

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

    HttpServer server = new HttpServer();

    // the server hosts what it is given; translating is the caller's business
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

  @Test
  void bindReportsWhatItGot() {
    assumeTrue(Odr.hasHttpServer(), "built without the HTTP server");

    HttpServer server = new HttpServer();

    // a literal address on purpose: "localhost" resolves to both ::1 and 127.0.0.1,
    // so a second bind would land on the other one instead of colliding
    int port = server.bind("127.0.0.1", 0);
    assertNotEquals(0, port);

    // a second bind would leak the first socket, so it is refused
    assertThrows(RuntimeException.class, () -> server.bind("127.0.0.1", 0));

    server.stop();
  }

  /** Starts a listen thread and returns once its accept loop is up. */
  private static Thread listenInBackground(HttpServer server) throws InterruptedException {
    Thread thread = new Thread(server::listen);
    thread.setDaemon(true);
    thread.start();
    while (!server.isRunning()) {
      Thread.sleep(1);
    }
    return thread;
  }

  @Test
  void stopWaitsForListen() throws Exception {
    assumeTrue(Odr.hasHttpServer(), "built without the HTTP server");

    HttpServer server = new HttpServer();
    server.bind("127.0.0.1", 0);
    Thread thread = listenInBackground(server);

    server.stop();
    // the accept loop is gone for good by now, which is what makes freeing the
    // native server right after safe
    assertFalse(server.isRunning());

    thread.join(Duration.ofSeconds(5).toMillis());
    assertFalse(thread.isAlive());
  }

  @Test
  void closeStopsListen() throws Exception {
    assumeTrue(Odr.hasHttpServer(), "built without the HTTP server");

    Thread thread;
    try (HttpServer server = new HttpServer()) {
      server.bind("127.0.0.1", 0);
      thread = listenInBackground(server);
    }

    // close() stopped the server before freeing it, rather than pulling it out
    // from under the accept loop
    thread.join(Duration.ofSeconds(5).toMillis());
    assertFalse(thread.isAlive());
  }

  @Test
  void closeRacingTheStartOfListenIsSafe() throws Exception {
    assumeTrue(Odr.hasHttpServer(), "built without the HTTP server");

    // deliberately no wait for isRunning(): close() may get here while the listen
    // thread has the handle but has not reached the native call, where stopping
    // alone would find nothing to wait for and free the server underneath it
    for (int i = 0; i < 20; i++) {
      HttpServer server = new HttpServer();
      server.bind("127.0.0.1", 0);

      Thread thread = new Thread(server::listen);
      thread.setDaemon(true);
      thread.start();

      server.close();

      thread.join(Duration.ofSeconds(5).toMillis());
      assertFalse(thread.isAlive());
    }
  }

  @Test
  void listenWithoutBindThrows() {
    assumeTrue(Odr.hasHttpServer(), "built without the HTTP server");

    HttpServer server = new HttpServer();

    // cpp-httplib reports success for this, hence the guard being tested
    assertThrows(RuntimeException.class, server::listen);
  }
}
