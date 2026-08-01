package app.opendocument.core

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.nio.file.Files
import java.nio.file.Path
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * The HTTP server on a device. It is the one part of the bindings that runs the library on threads
 * cpp-httplib started rather than on a java one, which is where the android JNI implementation is
 * least forgiving (#628).
 */
@RunWith(AndroidJUnit4::class)
class HttpServerTest {
    private lateinit var tempDir: Path

    @Before
    fun setUp() {
        tempDir = TestSupport.tempDir("http-server")
    }

    private fun fetch(url: String): String {
        val deadline = System.nanoTime() + 5_000_000_000L
        while (true) {
            val connection = URL(url).openConnection() as HttpURLConnection
            connection.connectTimeout = 1000
            connection.readTimeout = 1000
            // no keep-alive connection may outlive the request: cpp-httplib's
            // listen() only returns once its workers are done
            connection.setRequestProperty("Connection", "close")
            try {
                // an AssertionError, unlike an IOException, is not worth retrying
                assertEquals(200, connection.responseCode)
                return connection.inputStream.use { it.readBytes().toString(Charsets.UTF_8) }
            } catch (e: IOException) {
                if (System.nanoTime() > deadline) {
                    throw e
                }
                Thread.sleep(50)
            } finally {
                connection.disconnect()
            }
        }
    }

    @Test
    fun serveFile() {
        assumeTrue("built without the HTTP server", Odr.hasHttpServer())

        val server = HttpServer()

        val cachePath = Files.createDirectories(tempDir.resolve("doc-cache")).toString()
        val file = Odr.open(TestFiles.odtFile(tempDir).toString())
        val htmlConfig = HtmlConfig()
        htmlConfig.embedImages = false
        htmlConfig.relativeResourcePaths = false
        val service = Html.translate(file, cachePath, htmlConfig)
        server.connectService(service, "doc")
        val views = service.listViews()
        assertEquals(1, views.size)

        val port = server.bind("127.0.0.1", 0)
        val listenError = AtomicReference<Throwable>()
        val thread = Thread {
            try {
                server.listen()
            } catch (t: Throwable) {
                listenError.set(t)
            }
        }
        thread.isDaemon = true
        thread.start()
        try {
            val body = fetch("http://127.0.0.1:$port/file/doc/${views[0].path()}")
            assertTrue(body.contains(TestFiles.ODT_WORD))
        } catch (e: Exception) {
            listenError.get()?.let { throw AssertionError("listen failed", it) }
            throw e
        } finally {
            server.stop()
            thread.join(5000)
        }
        assertFalse(thread.isAlive)
    }

    /**
     * Tearing the server down while its accept loop is up. android is where this is least
     * forgiving: fdsan aborts the process when the listening socket is closed twice, and both
     * teardown races showed up in an instrumented run (#631).
     */
    @Test
    fun closeStopsListen() {
        assumeTrue("built without the HTTP server", Odr.hasHttpServer())

        val server = HttpServer()
        server.bind("127.0.0.1", 0)

        val thread = Thread { server.listen() }
        thread.isDaemon = true
        thread.start()
        while (!server.isRunning) {
            Thread.sleep(1)
        }

        // close() stops the server before freeing it, rather than pulling it out from
        // under the accept loop
        server.close()

        thread.join(5000)
        assertFalse(thread.isAlive)
    }

    @Test
    fun bindReportsWhatItGot() {
        assumeTrue("built without the HTTP server", Odr.hasHttpServer())

        val server = HttpServer()
        // a literal address on purpose: "localhost" resolves to both ::1 and
        // 127.0.0.1, so a second bind would land on the other one
        val port = server.bind("127.0.0.1", 0)
        assertNotEquals(0, port)
        server.stop()
    }
}
