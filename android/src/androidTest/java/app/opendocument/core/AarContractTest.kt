package app.opendocument.core

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith

/** The AAR's own contract: the native library loads and works without any setup. */
@RunWith(AndroidJUnit4::class)
class AarContractTest {
    @Test
    fun nativeLibraryLoads() {
        // reaching the native side at all means the .so, its ABI and libc++_shared
        // are all where the packaging put them
        assertFalse(Odr.identify().isEmpty())
        assertNotNull(Odr.version())
        assertNotNull(Odr.commitHash())
    }

    @Test
    fun detectsTypeInsideTheContainer() {
        val directory = TestSupport.tempDir("magic")
        val odt = TestFiles.odtFile(directory)

        // naming the odt rather than the zip holding it means detection opened the
        // container
        assertEquals("application/vnd.oasis.opendocument.text", Odr.mimetype(odt.toString()))
    }
}
