package app.opendocument.core

import androidx.test.ext.junit.runners.AndroidJUnit4
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Decoding and rendering on a device, mirroring the host suite in `jni/tests`. What is android
 * specific here is the runtime: every one of these calls crosses into the same java API on an
 * android class library, where a method the JDK has and android does not fails only when it is
 * reached (#621).
 */
@RunWith(AndroidJUnit4::class)
class DocumentTest {
    private lateinit var tempDir: Path

    @Before
    fun setUp() {
        TestSupport.initialize()
        tempDir = TestSupport.tempDir("document")
    }

    private fun openDocument(): Document {
        val odt = TestFiles.odtFile(tempDir)
        return Odr.open(odt.toString()).asDocumentFile().document()
    }

    private fun walkText(element: Element): List<String> = buildList {
        if (element.type() == ElementType.TEXT) {
            add(element.asText().content())
        }
        for (child in element.children()) {
            addAll(walkText(child))
        }
    }

    @Test
    fun elementTree() {
        val document = openDocument()
        assertEquals(DocumentType.TEXT, document.documentType())
        assertEquals(FileType.OPENDOCUMENT_TEXT, document.fileType())

        val root = document.rootElement()
        assertEquals(ElementType.ROOT, root.type())

        val text = walkText(root)
        assertTrue(text.contains(TestFiles.ODT_FIRST_PARAGRAPH))
        // exercises non-BMP characters across the JNI string conversion
        assertTrue(text.contains(TestFiles.ODT_SECOND_PARAGRAPH))
    }

    @Test
    fun elementNavigation() {
        val root = openDocument().rootElement()

        val first = root.firstChild()
        assertNotNull(first)
        assertTrue(first.parent().isSame(root))
        val second = first.nextSibling()
        assertNotNull(second)
        assertTrue(second.previousSibling().isSame(first))
        assertNotNull(first.documentPath().toString())
    }

    @Test
    fun documentFilesystem() {
        assertTrue(openDocument().asFilesystem().isFile("/content.xml"))
    }

    @Test
    fun fileMeta() {
        Odr.open(TestFiles.odtFile(tempDir).toString()).use { file ->
            val meta = file.fileMeta()
            assertEquals(FileType.OPENDOCUMENT_TEXT, meta.type)
            assertFalse(meta.passwordEncrypted)
            assertEquals(EncryptionState.NOT_ENCRYPTED, file.encryptionState())
        }
    }

    @Test
    fun translateToHtml() {
        val cache = Files.createDirectories(tempDir.resolve("cache"))
        val output = Files.createDirectories(tempDir.resolve("output"))

        val file = Odr.open(TestFiles.odtFile(tempDir).toString())
        val service = Html.translate(file, cache.toString(), HtmlConfig())
        val html = service.bringOffline(output.toString())

        val pages = html.pages()
        assertEquals(1, pages.size)
        // the renderer reads the css/js the AAR ships, so this only passes with the
        // extracted assets in place
        val content = read(Paths.get(pages[0].path))
        assertTrue(content.contains(TestFiles.ODT_FIRST_PARAGRAPH))
    }

    @Test
    fun translateCsv() {
        val cache = Files.createDirectories(tempDir.resolve("csv-cache"))
        val file = Odr.open(TestFiles.csvFile(tempDir).toString())
        val service = Html.translate(file, cache.toString(), HtmlConfig())

        val views = service.listViews()
        assertEquals(1, views.size)
        assertTrue(views[0].writeHtml().html.contains("alpha"))
    }

    private fun read(path: Path): String = String(Files.readAllBytes(path), StandardCharsets.UTF_8)
}
