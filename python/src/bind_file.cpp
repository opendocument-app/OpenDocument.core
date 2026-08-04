#include "bindings.hpp"

#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/logger.hpp>

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
      .value("xml", odr::FileType::xml);

  py::enum_<odr::FileCategory>(m, "FileCategory")
      .value("unknown", odr::FileCategory::unknown)
      .value("text", odr::FileCategory::text)
      .value("image", odr::FileCategory::image)
      .value("archive", odr::FileCategory::archive)
      .value("document", odr::FileCategory::document)
      .value("font", odr::FileCategory::font)
      .value("audio", odr::FileCategory::audio)
      .value("video", odr::FileCategory::video);

  py::enum_<odr::FileLocation>(m, "FileLocation")
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

  py::class_<odr::DecodePreference>(m, "DecodePreference")
      .def(py::init<>())
      .def_readwrite("as_file_type", &odr::DecodePreference::as_file_type)
      .def_readwrite("file_type_priority",
                     &odr::DecodePreference::file_type_priority);

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
      .def_readwrite("edit", &odr::FileTypeCapabilities::edit)
      .def_readwrite("save", &odr::FileTypeCapabilities::save)
      .def_readwrite("encrypt", &odr::FileTypeCapabilities::encrypt);

  py::class_<odr::File>(m, "File")
      .def(py::init<>())
      .def(py::init<const std::string &>(), py::arg("path"))
      .def("__bool__",
           [](const odr::File &file) { return file.impl() != nullptr; })
      .def("location", &odr::File::location)
      .def("size", &odr::File::size)
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

  py::class_<odr::DecodedFile>(m, "DecodedFile")
      .def(py::init<const odr::File &, const odr::Logger &>(), py::arg("file"),
           py::arg("logger") = odr::Logger::null())
      .def(py::init<const std::string &, const odr::Logger &>(),
           py::arg("path"), py::arg("logger") = odr::Logger::null())
      .def(py::init<const std::string &, odr::FileType, const odr::Logger &>(),
           py::arg("path"), py::arg("as_type"),
           py::arg("logger") = odr::Logger::null())
      .def("file", &odr::DecodedFile::file)
      .def("file_type", &odr::DecodedFile::file_type)
      .def("file_category", &odr::DecodedFile::file_category)
      .def("file_meta", &odr::DecodedFile::file_meta)
      .def("password_encrypted", &odr::DecodedFile::password_encrypted)
      .def("encryption_state", &odr::DecodedFile::encryption_state)
      .def("decrypt", &odr::DecodedFile::decrypt, py::arg("password"))
      .def("is_decodable", &odr::DecodedFile::is_decodable)
      .def("capabilities", &odr::DecodedFile::capabilities)
      .def("is_text_file", &odr::DecodedFile::is_text_file)
      .def("is_image_file", &odr::DecodedFile::is_image_file)
      .def("is_archive_file", &odr::DecodedFile::is_archive_file)
      .def("is_document_file", &odr::DecodedFile::is_document_file)
      .def("is_pdf_file", &odr::DecodedFile::is_pdf_file)
      .def("is_font_file", &odr::DecodedFile::is_font_file)
      .def("as_text_file", &odr::DecodedFile::as_text_file)
      .def("as_image_file", &odr::DecodedFile::as_image_file)
      .def("as_archive_file", &odr::DecodedFile::as_archive_file)
      .def("as_document_file", &odr::DecodedFile::as_document_file)
      .def("as_pdf_file", &odr::DecodedFile::as_pdf_file)
      .def("as_font_file", &odr::DecodedFile::as_font_file);

  py::class_<odr::TextFile, odr::DecodedFile>(m, "TextFile")
      .def("charset", &odr::TextFile::charset)
      .def("text", &odr::TextFile::text);

  py::class_<odr::ImageFile, odr::DecodedFile>(m, "ImageFile")
      .def("read", [](const odr::ImageFile &file) {
        std::ostringstream out;
        out << file.stream()->rdbuf();
        return py::bytes(out.str());
      });

  py::class_<odr::ArchiveFile, odr::DecodedFile>(m, "ArchiveFile")
      .def("archive", &odr::ArchiveFile::archive);

  py::class_<odr::DocumentFile, odr::DecodedFile>(m, "DocumentFile")
      .def(py::init<const std::string &>(), py::arg("path"))
      .def_static("type_by_path", &odr::DocumentFile::type, py::arg("path"))
      .def_static("meta_by_path", &odr::DocumentFile::meta, py::arg("path"))
      .def("document_type", &odr::DocumentFile::document_type)
      .def("decrypt", &odr::DocumentFile::decrypt, py::arg("password"))
      .def("document", &odr::DocumentFile::document);

  py::class_<odr::PdfFile, odr::DecodedFile>(m, "PdfFile")
      .def("decrypt", &odr::PdfFile::decrypt, py::arg("password"));

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
