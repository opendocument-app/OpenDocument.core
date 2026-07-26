#include "bindings.hpp"

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/global_params.hpp>
#include <odr/odr.hpp>

#include <pybind11/stl.h>

#include <string>

namespace py = pybind11;

void odr_python::bind_core(py::module_ &m) {
  m.def("version", &odr::version, "Version of the underlying odrcore library.");
  m.def("commit_hash", &odr::commit_hash,
        "Git commit hash of the underlying odrcore library.");
  m.def("is_dirty", &odr::is_dirty);
  m.def("is_debug", &odr::is_debug);
  m.def("identify", &odr::identify,
        "Identification string of the underlying odrcore library.");

  py::class_<odr::GlobalParams>(m, "GlobalParams",
                                "Global resource paths of the library.")
      .def_static("odr_core_data_path", &odr::GlobalParams::odr_core_data_path)
      .def_static("libmagic_database_path",
                  &odr::GlobalParams::libmagic_database_path)
      .def_static("set_odr_core_data_path",
                  &odr::GlobalParams::set_odr_core_data_path, py::arg("path"))
      .def_static("set_libmagic_database_path",
                  &odr::GlobalParams::set_libmagic_database_path,
                  py::arg("path"));

  py::register_exception<odr::UnsupportedOperation>(m, "UnsupportedOperation");
  py::register_exception<odr::FileNotFound>(m, "FileNotFound",
                                            PyExc_FileNotFoundError);
  py::register_exception<odr::UnknownFileType>(m, "UnknownFileTypeError");
  py::register_exception<odr::UnsupportedFileType>(m,
                                                   "UnsupportedFileTypeError");
  py::register_exception<odr::UnknownDecoderEngine>(
      m, "UnknownDecoderEngineError");
  py::register_exception<odr::UnsupportedDecoderEngine>(
      m, "UnsupportedDecoderEngineError");
  py::register_exception<odr::FileReadError>(m, "FileReadError");
  py::register_exception<odr::FileWriteError>(m, "FileWriteError");
  py::register_exception<odr::NoDocumentFile>(m, "NoDocumentFileError");
  py::register_exception<odr::UnknownDocumentType>(m,
                                                   "UnknownDocumentTypeError");
  py::register_exception<odr::UnsupportedCryptoAlgorithm>(
      m, "UnsupportedCryptoAlgorithmError");
  py::register_exception<odr::WrongPasswordError>(m, "WrongPasswordError");
  py::register_exception<odr::DecryptionFailed>(m, "DecryptionFailedError");
  py::register_exception<odr::NotEncryptedError>(m, "NotEncryptedError");
  py::register_exception<odr::FileEncryptedError>(m, "FileEncryptedError");
  py::register_exception<odr::DocumentCopyProtectedException>(
      m, "DocumentCopyProtectedError");
}

void odr_python::bind_functions(py::module_ &m) {
  m.def(
      "file_type_by_file_extension",
      [](const std::string &extension) {
        return odr::file_type_by_file_extension(extension);
      },
      py::arg("extension"));
  m.def("file_category_by_file_type", &odr::file_category_by_file_type,
        py::arg("type"));
  m.def("document_type_by_file_type", &odr::document_type_by_file_type,
        py::arg("type"));
  m.def("file_type_to_string", &odr::file_type_to_string, py::arg("type"));
  m.def("file_category_to_string", &odr::file_category_to_string,
        py::arg("category"));
  m.def("document_type_to_string", &odr::document_type_to_string,
        py::arg("type"));
  m.def(
      "file_type_by_mimetype",
      [](const std::string &mimetype) {
        return odr::file_type_by_mimetype(mimetype);
      },
      py::arg("mimetype"));
  m.def(
      "mimetype_by_file_type",
      [](const odr::FileType type) {
        return std::string(odr::mimetype_by_file_type(type));
      },
      py::arg("type"));
  m.def("decoder_engine_to_string", &odr::decoder_engine_to_string,
        py::arg("engine"));
  m.def(
      "decoder_engine_by_name",
      [](const std::string &name) { return odr::decoder_engine_by_name(name); },
      py::arg("name"));

  m.def(
      "list_file_types",
      [](const std::string &path) { return odr::list_file_types(path); },
      py::arg("path"), "Determine the possible file types of a file.");
  m.def("list_decoder_engines", &odr::list_decoder_engines, py::arg("as_type"));
  m.def(
      "mimetype",
      [](const std::string &path) { return std::string(odr::mimetype(path)); },
      py::arg("path"), "Determine the MIME type of a file.");

  m.def(
      "open", [](const std::string &path) { return odr::open(path); },
      py::arg("path"), "Open and decode a file.");
  m.def(
      "open",
      [](const std::string &path, const odr::FileType as) {
        return odr::open(path, as);
      },
      py::arg("path"), py::arg("as_type"),
      "Open and decode a file as a specific file type.");
  m.def(
      "open",
      [](const std::string &path, const odr::DecodePreference &preference) {
        return odr::open(path, preference);
      },
      py::arg("path"), py::arg("preference"),
      "Open and decode a file with a decode preference.");
}
