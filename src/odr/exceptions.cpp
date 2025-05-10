#include <odr/exceptions.hpp>

#include <odr/file.hpp>

namespace odr {

UnsupportedOperation::UnsupportedOperation()
    : std::runtime_error("unsupported operation") {}

FileNotFound::FileNotFound() : std::runtime_error("file not found") {}

FileNotFound::FileNotFound(const std::string &path)
    : std::runtime_error("file not found: " + path) {}

UnknownFileType::UnknownFileType() : std::runtime_error("unknown file type") {}

UnsupportedFileType::UnsupportedFileType(const FileType file_type)
    : std::runtime_error("unknown file type"), file_type{file_type} {}

UnknownDecoderEngine::UnknownDecoderEngine()
    : std::runtime_error("unknown decoder engine") {}

FileReadError::FileReadError() : std::runtime_error("file read error") {}

FileWriteError::FileWriteError() : std::runtime_error("file write error") {}

NoZipFile::NoZipFile() : std::runtime_error("not a zip file") {}

ZipSaveError::ZipSaveError() : std::runtime_error("zip save error") {}

CfbError::CfbError(const char *desc) : std::runtime_error(desc) {}

NoCfbFile::NoCfbFile() : CfbError("no cfb file") {}

CfbFileCorrupted::CfbFileCorrupted() : CfbError("cfb file corrupted") {}

NoTextFile::NoTextFile() : std::runtime_error("not a text file") {}

UnknownCharset::UnknownCharset() : std::runtime_error("unknown charset") {}

NoImageFile::NoImageFile() : std::runtime_error("not an image file") {}

NoArchiveFile::NoArchiveFile() : std::runtime_error("not an archive file") {}

NoDocumentFile::NoDocumentFile() : std::runtime_error("not a document file") {}

NoOpenDocumentFile::NoOpenDocumentFile()
    : std::runtime_error("not an open document file") {}

NoOfficeOpenXmlFile::NoOfficeOpenXmlFile()
    : std::runtime_error("not an office open xml file") {}

NoPdfFile::NoPdfFile() : std::runtime_error("not a pdf file") {}

NoXml::NoXml() : std::runtime_error("not xml") {}

UnsupportedCryptoAlgorithm::UnsupportedCryptoAlgorithm()
    : std::runtime_error("unsupported crypto algorithm") {}

NoSvmFile::NoSvmFile() : std::runtime_error("not a svm file") {}

MalformedSvmFile::MalformedSvmFile()
    : std::runtime_error("malformed svm file") {}

UnsupportedEndian::UnsupportedEndian()
    : std::runtime_error("unsupported endian") {}

MsUnsupportedCryptoAlgorithm::MsUnsupportedCryptoAlgorithm()
    : std::runtime_error("unsupported crypto algorithm") {}

UnknownDocumentType::UnknownDocumentType()
    : std::runtime_error("unknown document type") {}

InvalidPrefix::InvalidPrefix() : std::runtime_error("invalid prefix string") {}

InvalidPrefix::InvalidPrefix(const std::string &prefix)
    : std::runtime_error("invalid prefix string: " + prefix) {}

DocumentCopyProtectedException::DocumentCopyProtectedException()
    : std::runtime_error("document copy protection") {}

ResourceNotAccessible::ResourceNotAccessible()
    : std::runtime_error("resource not accessible") {}

ResourceNotAccessible::ResourceNotAccessible(const std::string &name,
                                             const std::string &path)
    : std::runtime_error("resource not accessible: " + name + " at " + path) {}

} // namespace odr
