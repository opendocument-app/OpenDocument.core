#include "bindings.hpp"

#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <pybind11/stl.h>

#include <sstream>
#include <string>

namespace py = pybind11;

void odr_python::bind_file(py::module_ &m) {
  py::enum_<odr::FileType>(m, "FileType")
      .value("unknown", odr::FileType::unknown)
      .value("opendocument_text", odr::FileType::opendocument_text)
      .value("opendocument_presentation",
             odr::FileType::opendocument_presentation)
      .value("opendocument_spreadsheet",
             odr::FileType::opendocument_spreadsheet)
      .value("opendocument_graphics", odr::FileType::opendocument_graphics)
      .value("office_open_xml_document",
             odr::FileType::office_open_xml_document)
      .value("office_open_xml_presentation",
             odr::FileType::office_open_xml_presentation)
      .value("office_open_xml_workbook",
             odr::FileType::office_open_xml_workbook)
      .value("office_open_xml_encrypted",
             odr::FileType::office_open_xml_encrypted)
      .value("excel_binary_workbook", odr::FileType::excel_binary_workbook)
      .value("legacy_word_document", odr::FileType::legacy_word_document)
      .value("legacy_powerpoint_presentation",
             odr::FileType::legacy_powerpoint_presentation)
      .value("legacy_excel_worksheets", odr::FileType::legacy_excel_worksheets)
      .value("word_perfect", odr::FileType::word_perfect)
      .value("rich_text_format", odr::FileType::rich_text_format)
      .value("portable_document_format",
             odr::FileType::portable_document_format)
      .value("text_file", odr::FileType::text_file)
      .value("comma_separated_values", odr::FileType::comma_separated_values)
      .value("javascript_object_notation",
             odr::FileType::javascript_object_notation)
      .value("markdown", odr::FileType::markdown)
      .value("zip", odr::FileType::zip)
      .value("compound_file_binary_format",
             odr::FileType::compound_file_binary_format)
      .value("portable_network_graphics",
             odr::FileType::portable_network_graphics)
      .value("graphics_interchange_format",
             odr::FileType::graphics_interchange_format)
      .value("jpeg", odr::FileType::jpeg)
      .value("bitmap_image_file", odr::FileType::bitmap_image_file)
      .value("starview_metafile", odr::FileType::starview_metafile)
      .value("truetype_font", odr::FileType::truetype_font)
      .value("opentype_font", odr::FileType::opentype_font)
      .value("webp", odr::FileType::webp)
      .value("tagged_image_file_format",
             odr::FileType::tagged_image_file_format)
      .value("high_efficiency_image_format",
             odr::FileType::high_efficiency_image_format)
      .value("av1_image_file_format", odr::FileType::av1_image_file_format)
      .value("mpeg_audio", odr::FileType::mpeg_audio)
      .value("mpeg4_audio", odr::FileType::mpeg4_audio)
      .value("ogg_audio", odr::FileType::ogg_audio)
      .value("waveform_audio", odr::FileType::waveform_audio)
      .value("free_lossless_audio_codec",
             odr::FileType::free_lossless_audio_codec)
      .value("mpeg4_video", odr::FileType::mpeg4_video)
      .value("quicktime_video", odr::FileType::quicktime_video)
      .value("third_generation_partnership_video",
             odr::FileType::third_generation_partnership_video)
      .value("matroska_video", odr::FileType::matroska_video)
      .value("audio_video_interleave", odr::FileType::audio_video_interleave)
      .value("scalable_vector_graphics",
             odr::FileType::scalable_vector_graphics)
      .value("windows_icon", odr::FileType::windows_icon)
      .value("jpeg_xl", odr::FileType::jpeg_xl)
      .value("jpeg_2000", odr::FileType::jpeg_2000)
      .value("photoshop_document", odr::FileType::photoshop_document)
      .value("windows_metafile", odr::FileType::windows_metafile)
      .value("enhanced_metafile", odr::FileType::enhanced_metafile)
      .value("xml", odr::FileType::xml)
      .value("iwork_pages", odr::FileType::iwork_pages)
      .value("iwork_numbers", odr::FileType::iwork_numbers)
      .value("iwork_keynote", odr::FileType::iwork_keynote)
      .value("hypertext_markup_language",
             odr::FileType::hypertext_markup_language);

  py::enum_<odr::FileCategory>(m, "FileCategory")
      .value("unknown", odr::FileCategory::unknown)
      .value("text", odr::FileCategory::text)
      .value("image", odr::FileCategory::image)
      .value("archive", odr::FileCategory::archive)
      .value("document", odr::FileCategory::document)
      .value("font", odr::FileCategory::font)
      .value("audio", odr::FileCategory::audio)
      .value("video", odr::FileCategory::video);

  py::enum_<odr::TextEncoding>(
      m, "TextEncoding",
      "A text encoding. Only some can be decoded - see\n"
      "`text_encoding_is_decodable`.")
      .value("unknown", odr::TextEncoding::unknown)
      .value("utf8", odr::TextEncoding::utf8)
      .value("utf16le", odr::TextEncoding::utf16le)
      .value("utf16be", odr::TextEncoding::utf16be)
      .value("utf32le", odr::TextEncoding::utf32le)
      .value("utf32be", odr::TextEncoding::utf32be)
      .value("ibm866", odr::TextEncoding::ibm866)
      .value("iso_8859_1", odr::TextEncoding::iso_8859_1)
      .value("iso_8859_2", odr::TextEncoding::iso_8859_2)
      .value("iso_8859_3", odr::TextEncoding::iso_8859_3)
      .value("iso_8859_4", odr::TextEncoding::iso_8859_4)
      .value("iso_8859_5", odr::TextEncoding::iso_8859_5)
      .value("iso_8859_6", odr::TextEncoding::iso_8859_6)
      .value("iso_8859_7", odr::TextEncoding::iso_8859_7)
      .value("iso_8859_8", odr::TextEncoding::iso_8859_8)
      .value("iso_8859_10", odr::TextEncoding::iso_8859_10)
      .value("iso_8859_13", odr::TextEncoding::iso_8859_13)
      .value("iso_8859_14", odr::TextEncoding::iso_8859_14)
      .value("iso_8859_15", odr::TextEncoding::iso_8859_15)
      .value("iso_8859_16", odr::TextEncoding::iso_8859_16)
      .value("koi8_r", odr::TextEncoding::koi8_r)
      .value("koi8_u", odr::TextEncoding::koi8_u)
      .value("macintosh", odr::TextEncoding::macintosh)
      .value("windows_874", odr::TextEncoding::windows_874)
      .value("windows_1250", odr::TextEncoding::windows_1250)
      .value("windows_1251", odr::TextEncoding::windows_1251)
      .value("windows_1252", odr::TextEncoding::windows_1252)
      .value("windows_1253", odr::TextEncoding::windows_1253)
      .value("windows_1254", odr::TextEncoding::windows_1254)
      .value("windows_1255", odr::TextEncoding::windows_1255)
      .value("windows_1256", odr::TextEncoding::windows_1256)
      .value("windows_1257", odr::TextEncoding::windows_1257)
      .value("windows_1258", odr::TextEncoding::windows_1258)
      .value("x_mac_cyrillic", odr::TextEncoding::x_mac_cyrillic)
      .value("big5", odr::TextEncoding::big5)
      .value("euc_jp", odr::TextEncoding::euc_jp)
      .value("euc_kr", odr::TextEncoding::euc_kr)
      .value("gb18030", odr::TextEncoding::gb18030)
      .value("iso_2022_jp", odr::TextEncoding::iso_2022_jp)
      .value("iso_2022_kr", odr::TextEncoding::iso_2022_kr)
      .value("shift_jis", odr::TextEncoding::shift_jis);

  py::enum_<odr::FileLocation>(m, "FileLocation")
      .value("unknown", odr::FileLocation::unknown)
      .value("memory", odr::FileLocation::memory)
      .value("disk", odr::FileLocation::disk);

  py::enum_<odr::EncryptionState>(m, "EncryptionState")
      .value("unknown", odr::EncryptionState::unknown)
      .value("not_encrypted", odr::EncryptionState::not_encrypted)
      .value("encrypted", odr::EncryptionState::encrypted)
      .value("decrypted", odr::EncryptionState::decrypted);

  py::enum_<odr::DocumentType>(m, "DocumentType")
      .value("unknown", odr::DocumentType::unknown)
      .value("text", odr::DocumentType::text)
      .value("presentation", odr::DocumentType::presentation)
      .value("spreadsheet", odr::DocumentType::spreadsheet)
      .value("drawing", odr::DocumentType::drawing);

  py::class_<odr::CsvOptions>(m, "CsvOptions",
                              "How to read a csv file. An unset field is "
                              "detected from the file's opening bytes.")
      .def(py::init([](std::optional<odr::TextEncoding> encoding,
                       std::optional<char> separator,
                       std::optional<char> quote) {
             return odr::CsvOptions{encoding, separator, quote};
           }),
           py::arg("encoding") = py::none(), py::arg("separator") = py::none(),
           py::arg("quote") = py::none())
      .def_readwrite("encoding", &odr::CsvOptions::encoding)
      .def_readwrite("separator", &odr::CsvOptions::separator)
      .def_readwrite("quote", &odr::CsvOptions::quote);

  py::class_<odr::DecodeOptions>(m, "DecodeOptions",
                                 "How to decode a file. Every field is "
                                 "optional; the default detects everything.")
      .def(py::init([](std::optional<odr::FileType> as_file_type,
                       std::vector<odr::FileType> file_type_priority,
                       odr::CsvOptions csv) {
             return odr::DecodeOptions{
                 as_file_type, std::move(file_type_priority), std::move(csv)};
           }),
           py::arg("as_file_type") = py::none(),
           py::arg("file_type_priority") = std::vector<odr::FileType>{},
           py::arg("csv") = odr::CsvOptions{})
      .def_readwrite("as_file_type", &odr::DecodeOptions::as_file_type)
      .def_readwrite("file_type_priority",
                     &odr::DecodeOptions::file_type_priority)
      .def_readwrite("csv", &odr::DecodeOptions::csv);

  py::class_<odr::FileMeta>(m, "FileMeta")
      .def(py::init<>())
      .def_readwrite("type", &odr::FileMeta::type)
      .def_property_readonly(
          "mimetype",
          [](const odr::FileMeta &meta) { return std::string(meta.mimetype); })
      .def_readwrite("password_encrypted", &odr::FileMeta::password_encrypted)
      .def_readwrite("document_type", &odr::FileMeta::document_type)
      .def_readwrite("entry_count", &odr::FileMeta::entry_count)
      .def_readwrite("title", &odr::FileMeta::title)
      .def_readwrite("author", &odr::FileMeta::author)
      .def_readwrite("subject", &odr::FileMeta::subject)
      .def_readwrite("keywords", &odr::FileMeta::keywords)
      .def_readwrite("creator", &odr::FileMeta::creator)
      .def_readwrite("producer", &odr::FileMeta::producer)
      .def_readwrite("creation_date", &odr::FileMeta::creation_date)
      .def_readwrite("modification_date", &odr::FileMeta::modification_date);

  py::class_<odr::FileTypeCapabilities>(m, "FileTypeCapabilities")
      .def(py::init<>())
      .def_readwrite("detect_by_content",
                     &odr::FileTypeCapabilities::detect_by_content)
      .def_readwrite("open", &odr::FileTypeCapabilities::open)
      .def_readwrite("decrypt", &odr::FileTypeCapabilities::decrypt)
      .def_readwrite("translate_html",
                     &odr::FileTypeCapabilities::translate_html)
      .def_readwrite("color_scheme", &odr::FileTypeCapabilities::color_scheme)
      .def_readwrite("edit", &odr::FileTypeCapabilities::edit)
      .def_readwrite("save", &odr::FileTypeCapabilities::save)
      .def_readwrite("encrypt", &odr::FileTypeCapabilities::encrypt)
      .def_readwrite("annotate", &odr::FileTypeCapabilities::annotate);

  py::class_<odr::File>(m, "File")
      .def(py::init<>())
      .def(py::init<const std::string &>(), py::arg("path"))
      .def_static("from_disk", &odr::File::from_disk, py::arg("path"),
                  "A file read from `path` on disk.")
      .def_static(
          "from_memory",
          [](const py::bytes &data, std::string name) {
            return odr::File::from_memory(std::string(data), std::move(name));
          },
          py::arg("data"), py::arg("name") = std::string(),
          "A file held in memory; `data` is its bytes, `name` what it is "
          "called, if known.")
      .def("__bool__",
           [](const odr::File &file) { return file.impl() != nullptr; })
      .def("location", &odr::File::location)
      .def("size", &odr::File::size)
      .def("name", &odr::File::name,
           "The file name, without any directory; empty where there is none.")
      .def("disk_path", &odr::File::disk_path)
      .def(
          "read",
          [](const odr::File &file) {
            std::ostringstream out;
            file.pipe(out);
            return py::bytes(out.str());
          },
          "Read the whole file into bytes.")
      .def("copy", &odr::File::copy, py::arg("path"));

  // no constructor: `pyodr.open(...)` decodes, this is what it hands back
  py::class_<odr::DecodedFile>(m, "DecodedFile")
      .def("file", &odr::DecodedFile::file)
      .def("file_type", &odr::DecodedFile::file_type)
      .def("file_category", &odr::DecodedFile::file_category)
      .def("file_meta", &odr::DecodedFile::file_meta)
      .def("password_encrypted", &odr::DecodedFile::password_encrypted)
      .def("encryption_state", &odr::DecodedFile::encryption_state)
      // decrypting rewrites the whole file; holding the GIL for it blocks
      // every other Python thread
      .def("decrypt", &odr::DecodedFile::decrypt, py::arg("password"),
           py::call_guard<py::gil_scoped_release>())
      .def("is_decodable", &odr::DecodedFile::is_decodable)
      .def("capabilities", &odr::DecodedFile::capabilities)
      .def("is_text_file", &odr::DecodedFile::is_text_file)
      .def("is_csv_file", &odr::DecodedFile::is_csv_file)
      .def("is_image_file", &odr::DecodedFile::is_image_file)
      .def("is_archive_file", &odr::DecodedFile::is_archive_file)
      .def("is_document_file", &odr::DecodedFile::is_document_file)
      .def("is_pdf_file", &odr::DecodedFile::is_pdf_file)
      .def("is_font_file", &odr::DecodedFile::is_font_file)
      .def("as_text_file", &odr::DecodedFile::as_text_file)
      .def("as_csv_file", &odr::DecodedFile::as_csv_file)
      .def("as_image_file", &odr::DecodedFile::as_image_file)
      .def("as_archive_file", &odr::DecodedFile::as_archive_file)
      .def("as_document_file", &odr::DecodedFile::as_document_file)
      .def("as_pdf_file", &odr::DecodedFile::as_pdf_file)
      .def("as_font_file", &odr::DecodedFile::as_font_file);

  // A csv is a text file too, so `CsvFile` derives from `TextFile` the way the
  // C++ handle does - `text()` still reads the raw bytes.
  py::class_<odr::CsvFile, odr::DecodedFile>(m, "CsvFile")
      .def("options", &odr::CsvFile::options,
           "The options in use, every field resolved.")
      .def("document", &odr::CsvFile::document,
           "The csv as a one-sheet spreadsheet.");

  py::class_<odr::TextFile, odr::DecodedFile>(m, "TextFile")
      .def("encoding", &odr::TextFile::encoding,
           "The encoding the bytes were detected as, or decoded with.")
      .def("charset",
           [](const odr::TextFile &file) -> std::optional<std::string> {
             const odr::TextEncoding encoding = file.encoding();
             if (encoding == odr::TextEncoding::unknown) {
               return {};
             }
             return std::string(odr::text_encoding_to_string(encoding));
           })
      .def("text", &odr::TextFile::text);

  py::class_<odr::ImageFile, odr::DecodedFile>(m, "ImageFile")
      .def("read", [](const odr::ImageFile &file) {
        std::ostringstream out;
        out << file.stream()->rdbuf();
        return py::bytes(out.str());
      });

  py::class_<odr::ArchiveFile, odr::DecodedFile>(m, "ArchiveFile")
      .def("archive", &odr::ArchiveFile::archive);

  // no constructor either: `pyodr.open(...).as_document_file()` narrows
  py::class_<odr::DocumentFile, odr::DecodedFile>(m, "DocumentFile")
      .def("document_type", &odr::DocumentFile::document_type)
      .def("decrypt", &odr::DocumentFile::decrypt, py::arg("password"),
           py::call_guard<py::gil_scoped_release>())
      .def("thumbnail", &odr::DocumentFile::thumbnail,
           "The preview image the package carries, or `None` where it "
           "carries none or is still encrypted. Never rendered by us.")
      .def("document", &odr::DocumentFile::document);

  py::class_<odr::PdfFile, odr::DecodedFile>(m, "PdfFile")
      .def("is_annotatable", &odr::PdfFile::is_annotatable,
           "Whether this file can take annotations.")
      .def(
          "annotate",
          [](const odr::PdfFile &file, const std::string &annotations) {
            std::ostringstream out;
            {
              // scoped rather than a `call_guard`: the `py::bytes` below needs
              // the GIL back
              const py::gil_scoped_release release;
              file.annotate(annotations, out);
            }
            return py::bytes(std::move(out).str());
          },
          py::arg("annotations"),
          "Apply markup annotations and return the annotated pdf.")
      .def("decrypt", &odr::PdfFile::decrypt, py::arg("password"),
           py::call_guard<py::gil_scoped_release>());

  py::class_<odr::FontFile, odr::DecodedFile>(m, "FontFile")
      .def("read", [](const odr::FontFile &file) {
        std::ostringstream out;
        out << file.stream()->rdbuf();
        return py::bytes(out.str());
      });

  py::class_<odr::FileWalker>(m, "FileWalker")
      .def("end", &odr::FileWalker::end)
      .def("depth", &odr::FileWalker::depth)
      .def("path", &odr::FileWalker::path)
      .def("is_file", &odr::FileWalker::is_file)
      .def("is_directory", &odr::FileWalker::is_directory)
      .def("pop", &odr::FileWalker::pop)
      .def("next", &odr::FileWalker::next)
      .def("flat_next", &odr::FileWalker::flat_next);

  py::class_<odr::Filesystem>(m, "Filesystem")
      .def("exists", &odr::Filesystem::exists, py::arg("path"))
      .def("is_file", &odr::Filesystem::is_file, py::arg("path"))
      .def("is_directory", &odr::Filesystem::is_directory, py::arg("path"))
      .def("file_walker", &odr::Filesystem::file_walker, py::arg("path"))
      .def("open", &odr::Filesystem::open, py::arg("path"));

  py::class_<odr::Archive>(m, "Archive")
      .def("as_filesystem", &odr::Archive::as_filesystem)
      .def("save", [](const odr::Archive &archive) {
        std::ostringstream out;
        archive.save(out);
        return py::bytes(out.str());
      });
}
