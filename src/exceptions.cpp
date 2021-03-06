#include <odr/exceptions.h>

namespace odr {

UnsupportedOperation::UnsupportedOperation()
    : std::runtime_error("unsupported operation") {}

FileNotFound::FileNotFound() : std::runtime_error("file not found") {}

FileNotCreated::FileNotCreated() : std::runtime_error("file not created") {}

UnknownFileType::UnknownFileType() : std::runtime_error("unknown file type") {}

FileReadError::FileReadError() : std::runtime_error("file read error") {}

NoZipFile::NoZipFile() : std::runtime_error("not a zip file") {}

ZipSaveError::ZipSaveError() : std::runtime_error("zip save error") {}

CfbError::CfbError(const char *desc) : std::runtime_error(desc) {}

NoCfbFile::NoCfbFile() : CfbError("no cfb file") {}

CfbFileCorrupted::CfbFileCorrupted() : CfbError("cfb file corrupted") {}

NoImageFile::NoImageFile() : std::runtime_error("not an image file") {}

NoDocumentFile::NoDocumentFile() : std::runtime_error("not a document file") {}

NoOpenDocumentFile::NoOpenDocumentFile()
    : std::runtime_error("not an open document file") {}

NoXml::NoXml() : std::runtime_error("not xml") {}

PropertyNotOptional::PropertyNotOptional()
    : std::runtime_error("property not optional") {}

PropertyReadOnly::PropertyReadOnly()
    : std::runtime_error("property is readonly") {}

UnsupportedCryptoAlgorithm::UnsupportedCryptoAlgorithm()
    : std::runtime_error("unsupported crypto algorithm") {}

NoSvmFile::NoSvmFile() : std::runtime_error("not a svm file") {}

MalformedSvmFile::MalformedSvmFile()
    : std::runtime_error("malformed svm file") {}

UnsupportedEndian::UnsupportedEndian()
    : std::runtime_error("unsupported endian") {}

MsUnsupportedCryptoAlgorithm::MsUnsupportedCryptoAlgorithm()
    : std::runtime_error("unsupported crypto algorithm") {}

} // namespace odr
