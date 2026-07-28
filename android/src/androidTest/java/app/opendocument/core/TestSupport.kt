package app.opendocument.core

import android.content.Context
import androidx.test.platform.app.InstrumentationRegistry
import app.opendocument.core.android.OdrAndroid
import java.io.File
import java.io.IOException
import java.nio.file.Path

/** Shared setup of the instrumented suite: the initialised library and a scratch directory. */
internal object TestSupport {
    fun context(): Context = InstrumentationRegistry.getInstrumentation().targetContext

    /** The library, with its bundled assets extracted and registered. */
    fun initialize() {
        OdrAndroid.init(context())
    }

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
