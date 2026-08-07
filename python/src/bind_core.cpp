#include "bindings.hpp"

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/global_params.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <pybind11/stl.h>

#include <string>
#include <vector>

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
      // the two libmagic paths are deprecated and inert: libmagic is gone, so
      // nothing reads back what they store
      .def_static("libmagic_database_path",
                  &odr::GlobalParams::libmagic_database_path)
      .def_static("set_odr_core_data_path",
                  &odr::GlobalParams::set_odr_core_data_path, py::arg("path"))
      .def_static("set_libmagic_database_path",
                  &odr::GlobalParams::set_libmagic_database_path,
                  py::arg("path"));

  // Mirrors odr::Exception, so `except odr.Error` catches the whole library.
  const py::exception<odr::Exception> error(m, "Error");

  py::register_exception<odr::UnsupportedOperation>(m, "UnsupportedOperation",
                                                    error);
  // The one deliberate exception: mapping onto Python's own FileNotFoundError
  // is worth more here than sitting under `Error`.
  py::register_exception<odr::FileNotFound>(m, "FileNotFound",
                                            PyExc_FileNotFoundError);
  py::register_exception<odr::UnknownFileType>(m, "UnknownFileTypeError",
                                               error);
  py::register_exception<odr::UnsupportedFileType>(
      m, "UnsupportedFileTypeError", error);
  py::register_exception<odr::FileReadError>(m, "FileReadError", error);
  py::register_exception<odr::FileWriteError>(m, "FileWriteError", error);
  py::register_exception<odr::NoDocumentFile>(m, "NoDocumentFileError", error);
  py::register_exception<odr::UnknownDocumentType>(
      m, "UnknownDocumentTypeError", error);
  py::register_exception<odr::UnsupportedCryptoAlgorithm>(
      m, "UnsupportedCryptoAlgorithmError", error);
  py::register_exception<odr::WrongPasswordError>(m, "WrongPasswordError",
                                                  error);
  py::register_exception<odr::DecryptionFailed>(m, "DecryptionFailedError",
                                                error);
  py::register_exception<odr::NotEncryptedError>(m, "NotEncryptedError", error);
  py::register_exception<odr::FileEncryptedError>(m, "FileEncryptedError",
                                                  error);
  py::register_exception<odr::DocumentCopyProtectedException>(
      m, "DocumentCopyProtectedError", error);
}

void odr_python::bind_functions(py::module_ &m) {
  m.def("all_file_types", &odr::all_file_types,
        "Every file type this library knows about.");
  m.def("file_type_by_file_extension", &odr::file_type_by_file_extension,
        py::arg("extension"));
  m.def(
      "file_extensions_by_file_type",
      [](const odr::FileType type) {
        const auto extensions = odr::file_extensions_by_file_type(type);
        return std::vector<std::string>(extensions.begin(), extensions.end());
      },
      py::arg("type"), "Every file extension accepted for the file type.");
  m.def(
      "file_extension_by_file_type",
      [](const odr::FileType type) {
        return std::string(odr::file_extension_by_file_type(type));
      },
      py::arg("type"));
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
  m.def(
      "mimetypes_by_file_type",
      [](const odr::FileType type) {
        const auto mimetypes = odr::mimetypes_by_file_type(type);
        return std::vector<std::string>(mimetypes.begin(), mimetypes.end());
      },
      py::arg("type"), "Every MIME type accepted for the file type.");
  m.def("capabilities_by_file_type", &odr::capabilities_by_file_type,
        py::arg("type"), "What this library can do with the file type.");

  m.def(
      "list_file_types",
      [](const std::string &path, const odr::Logger &logger) {
        return odr::list_file_types(path, logger);
      },
      py::arg("path"), py::arg("logger") = odr::Logger::null(),
      "Determine the possible file types of a file.");
  m.def(
      "mimetype",
      [](const std::string &path, const odr::Logger &logger) {
        return std::string(odr::mimetype(path, logger));
      },
      py::arg("path"), py::arg("logger") = odr::Logger::null(),
      "Determine the MIME type of a file.");

  m.def(
      "open",
      [](const std::string &path, const odr::Logger &logger) {
        return odr::open(path, logger);
      },
      py::arg("path"), py::arg("logger") = odr::Logger::null(),
      "Open and decode a file.");
  m.def(
      "open",
      [](const std::string &path, const odr::FileType as,
         const odr::Logger &logger) { return odr::open(path, as, logger); },
      py::arg("path"), py::arg("as_type"),
      py::arg("logger") = odr::Logger::null(),
      "Open and decode a file as a specific file type.");
  m.def(
      "open",
      [](const std::string &path, const odr::DecodePreference &preference,
         const odr::Logger &logger) {
        return odr::open(path, preference, logger);
      },
      py::arg("path"), py::arg("preference"),
      py::arg("logger") = odr::Logger::null(),
      "Open and decode a file with a decode preference.");
}
