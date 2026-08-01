package app.opendocument.core

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.nio.file.Path
import java.util.Collections
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * A java [ILogger] driven from native code, on ART. The sink is reached through a global reference
 * and cached method handles, and the calls arrive on whatever thread the library works on — the two
 * places where the android JNI implementation is stricter than the JDK's (#628).
 */
@RunWith(AndroidJUnit4::class)
class LoggerTest {
    private lateinit var tempDir: Path

    @Before
    fun setUp() {
        tempDir = TestSupport.tempDir("logger")
    }

    private class Collecting(private val level: LogLevel) : ILogger {
        val messages: MutableList<String> = Collections.synchronizedList(ArrayList())
        val threads: MutableList<String> = Collections.synchronizedList(ArrayList())
        @Volatile var flushes = 0

        override fun willLog(level: LogLevel): Boolean = level.ordinal >= this.level.ordinal

        override fun log(
            epochMillis: Long,
            level: LogLevel,
            message: String,
            location: SourceLocation,
        ) {
            messages.add(message)
            threads.add(Thread.currentThread().name)
        }

        override fun flush() {
            flushes++
        }
    }

    @Test
    fun customSinkReceivesMessages() {
        val sink = Collecting(LogLevel.WARNING)
        Logger(sink).use { logger ->
            assertFalse(logger.willLog(LogLevel.DEBUG))
            logger.log(LogLevel.DEBUG, "dropped")
            logger.log(LogLevel.ERROR, "kept")
            logger.flush()
        }

        assertEquals(listOf("kept"), sink.messages)
        assertEquals(1, sink.flushes)
    }

    @Test
    fun customSinkIsUsableWhileOpening() {
        val odt = TestFiles.odtFile(tempDir)
        val sink = Collecting(LogLevel.VERBOSE)
        Logger(sink).use { logger ->
            Odr.open(odt.toString(), logger).use { file ->
                assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileType())
            }
        }
        assertNotNull(sink.messages)
    }

    @Test
    fun sinkIsReachedFromABackgroundThread() {
        val sink = Collecting(LogLevel.VERBOSE)
        val done = CountDownLatch(1)
        Logger(sink).use { logger ->
            val thread =
                Thread(
                    {
                        logger.log(LogLevel.ERROR, "from a worker")
                        done.countDown()
                    },
                    "odr-log-worker",
                )
            thread.start()
            assertTrue(done.await(10, TimeUnit.SECONDS))
            thread.join()
        }

        assertEquals(listOf("from a worker"), sink.messages)
        assertEquals(listOf("odr-log-worker"), sink.threads)
    }

    @Test
    fun aThrowingSinkDoesNotDerailTheOperation() {
        val odt = TestFiles.odtFile(tempDir)
        val sink =
            object : ILogger {
                override fun willLog(level: LogLevel): Boolean = true

                override fun log(
                    epochMillis: Long,
                    level: LogLevel,
                    message: String,
                    location: SourceLocation,
                ) {
                    throw IllegalStateException("sink is broken")
                }

                override fun flush() {
                    throw IllegalStateException("sink is broken")
                }
            }

        // the exception is described and cleared on the native side; leaving it
        // pending would abort the next JNI call instead, which on ART kills the
        // process rather than failing the test
        Logger(sink).use { logger ->
            Odr.open(odt.toString(), logger).use { file ->
                assertEquals(FileType.OPENDOCUMENT_TEXT, file.fileType())
            }
        }
    }
}
