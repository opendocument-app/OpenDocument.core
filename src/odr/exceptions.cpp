#include <odr/exceptions.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

namespace odr {

UnsupportedOperation::UnsupportedOperation()
    : std::runtime_error("unsupported operation") {}

UnsupportedOperation::UnsupportedOperation(const std::string &message)
    : std::runtime_error("unsupported operation: " + message) {}

FileNotFound::FileNotFound() : std::runtime_error("file not found") {}

FileNotFound::FileNotFound(const std::string &path)
    : std::runtime_error("file not found: " + path) {}

UnknownFileType::UnknownFileType() : std::runtime_error("unknown file type") {}

UnsupportedFileType::UnsupportedFileType(const FileType file_type)
    : std::runtime_error("unknown file type: " +
                         file_type_to_string(file_type)),
      file_type{file_type} {}

UnknownDecoderEngine::UnknownDecoderEngine()
    : std::runtime_error("unknown decoder engine") {}

UnsupportedDecoderEngine::UnsupportedDecoderEngine(
    const DecoderEngine decoder_engine)
    : std::runtime_error("unsupported decoder engine: " +
                         decoder_engine_to_string(decoder_engine)),
      decoder_engine{decoder_engine} {}

FileReadError::FileReadError() : std::runtime_error("file read error") {}

FileWriteError::FileWriteError(const std::string &path)
    : std::runtime_error("file write error: " + path) {}

NoZipFile::NoZipFile() : std::runtime_error("not a zip file") {}

ZipSaveError::ZipSaveError() : std::runtime_error("zip save error") {}

CfbError::CfbError(const char *desc) : std::runtime_error(desc) {}

NoCfbFile::NoCfbFile() : CfbError("no cfb file") {}

CfbFileCorrupted::CfbFileCorrupted() : CfbError("cfb file corrupted") {}

NoTextFile::NoTextFile() : std::runtime_error("not a text file") {}

NoCsvFile::NoCsvFile() : std::runtime_error("not a csv file") {}

NoJsonFile::NoJsonFile() : std::runtime_error("not a json file") {}

UnknownCharset::UnknownCharset() : std::runtime_error("unknown charset") {}

NoImageFile::NoImageFile() : std::runtime_error("not an image file") {}

NoArchiveFile::NoArchiveFile() : std::runtime_error("not an archive file") {}

NoDocumentFile::NoDocumentFile() : std::runtime_error("not a document file") {}

NoOpenDocumentFile::NoOpenDocumentFile()
    : std::runtime_error("not an open document file") {}

NoOfficeOpenXmlFile::NoOfficeOpenXmlFile()
    : std::runtime_error("not an office open xml file") {}

NoPdfFile::NoPdfFile() : std::runtime_error("not a pdf file") {}

NoLegacyMicrosoftFile::NoLegacyMicrosoftFile()
    : std::runtime_error("not a legacy microsoft office file") {}

NoXmlFile::NoXmlFile() : std::runtime_error("not an xml file") {}

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

PrefixInUse::PrefixInUse() : std::runtime_error("prefix in use") {}

PrefixInUse::PrefixInUse(const std::string &prefix)
    : std::runtime_error("prefix in use: " + prefix) {}

UnsupportedOption::UnsupportedOption(const std::string &message)
    : std::runtime_error("unsupported option: " + message) {}

NullPointerError::NullPointerError(const std::string &variable)
    : std::runtime_error("null pointer error: " + variable) {}

WrongPasswordError::WrongPasswordError()
    : std::runtime_error("wrong password error") {}

DecryptionFailed::DecryptionFailed()
    : std::runtime_error("decryption failed") {}

NotEncryptedError::NotEncryptedError()
    : std::runtime_error("not encrypted error") {}

InvalidPath::InvalidPath(const std::string &message)
    : std::runtime_error("invalid path: " + message) {}

} // namespace odr
