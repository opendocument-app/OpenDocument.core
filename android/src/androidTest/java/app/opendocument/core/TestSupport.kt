package app.opendocument.core

import android.content.Context
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.io.IOException
import java.nio.file.Path

/** Shared setup of the instrumented suite. */
internal object TestSupport {
    fun context(): Context = InstrumentationRegistry.getInstrumentation().targetContext

    /** An empty directory under the app cache, named after the caller. */
    fun tempDir(name: String): Path {
        val directory = File(context().cacheDir, name)
        directory.deleteRecursively()
        if (!directory.mkdirs()) {
            throw IOException("could not create $directory")
        }
        return directory.toPath()
    }
}
