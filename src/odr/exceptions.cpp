#include <odr/exceptions.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

namespace odr {

UnsupportedOperation::UnsupportedOperation()
    : Exception("unsupported operation") {}

UnsupportedOperation::UnsupportedOperation(const std::string &message)
    : Exception("unsupported operation: " + message) {}

FileNotFound::FileNotFound() : Exception("file not found") {}

FileNotFound::FileNotFound(const std::string &path)
    : Exception("file not found: " + path) {}

UnknownFileType::UnknownFileType() : Exception("unknown file type") {}

UnsupportedFileType::UnsupportedFileType(const FileType file_type)
    : Exception("unsupported file type: " + file_type_to_string(file_type)),
      file_type{file_type} {}

FileReadError::FileReadError() : Exception("file read error") {}

FileWriteError::FileWriteError(const std::string &path)
    : Exception("file write error: " + path) {}

NoZipFile::NoZipFile() : Exception("not a zip file") {}

ZipSaveError::ZipSaveError() : Exception("zip save error") {}

CfbError::CfbError(const std::string &desc) : Exception(desc) {}

NoCfbFile::NoCfbFile() : CfbError("no cfb file") {}

CfbFileCorrupted::CfbFileCorrupted() : CfbError("cfb file corrupted") {}

NoTextFile::NoTextFile() : Exception("not a text file") {}

NoCsvFile::NoCsvFile() : Exception("not a csv file") {}

NoJsonFile::NoJsonFile() : Exception("not a json file") {}

UnknownCharset::UnknownCharset() : Exception("unknown charset") {}

NoImageFile::NoImageFile() : Exception("not an image file") {}

NoArchiveFile::NoArchiveFile() : Exception("not an archive file") {}

NoDocumentFile::NoDocumentFile() : Exception("not a document file") {}

NoOpenDocumentFile::NoOpenDocumentFile()
    : Exception("not an open document file") {}

NoOfficeOpenXmlFile::NoOfficeOpenXmlFile()
    : Exception("not an office open xml file") {}

NoPdfFile::NoPdfFile() : Exception("not a pdf file") {}

NoFontFile::NoFontFile() : Exception("not a font file") {}

NoLegacyMicrosoftFile::NoLegacyMicrosoftFile()
    : Exception("not a legacy microsoft office file") {}

NoXmlFile::NoXmlFile() : Exception("not an xml file") {}

UnsupportedCryptoAlgorithm::UnsupportedCryptoAlgorithm()
    : Exception("unsupported crypto algorithm") {}

NoSvmFile::NoSvmFile() : Exception("not a svm file") {}

MalformedSvmFile::MalformedSvmFile() : Exception("malformed svm file") {}

UnsupportedEndian::UnsupportedEndian() : Exception("unsupported endian") {}

MsUnsupportedCryptoAlgorithm::MsUnsupportedCryptoAlgorithm()
    : Exception("unsupported crypto algorithm") {}

UnknownDocumentType::UnknownDocumentType()
    : Exception("unknown document type") {}

InvalidPrefix::InvalidPrefix() : Exception("invalid prefix string") {}

InvalidPrefix::InvalidPrefix(const std::string &prefix)
    : Exception("invalid prefix string: " + prefix) {}

DocumentCopyProtectedException::DocumentCopyProtectedException()
    : Exception("document copy protection") {}

ResourceNotAccessible::ResourceNotAccessible()
    : Exception("resource not accessible") {}

ResourceNotAccessible::ResourceNotAccessible(const std::string &name,
                                             const std::string &path)
    : Exception("resource not accessible: " + name + " at " + path) {}

PrefixInUse::PrefixInUse() : Exception("prefix in use") {}

PrefixInUse::PrefixInUse(const std::string &prefix)
    : Exception("prefix in use: " + prefix) {}

ServerBindFailed::ServerBindFailed(const std::string &host,
                                   const std::uint32_t port)
    : Exception("server bind failed: " + host + ":" + std::to_string(port)) {}

ServerAlreadyBound::ServerAlreadyBound()
    : Exception("server is bound already") {}

ServerNotBound::ServerNotBound() : Exception("server is not bound") {}

UnsupportedOption::UnsupportedOption(const std::string &message)
    : Exception("unsupported option: " + message) {}

NullPointerError::NullPointerError(const std::string &variable)
    : Exception("null pointer error: " + variable) {}

WrongPasswordError::WrongPasswordError() : Exception("wrong password error") {}

DecryptionFailed::DecryptionFailed() : Exception("decryption failed") {}

NotEncryptedError::NotEncryptedError() : Exception("not encrypted error") {}

InvalidPath::InvalidPath(const std::string &message)
    : Exception("invalid path: " + message) {}

UnsupportedFileEncoding::UnsupportedFileEncoding(const std::string &message)
    : Exception("unsupported file encoding: " + message) {}

FileEncryptedError::FileEncryptedError() : Exception("file encrypted error") {}

UnauthenticatedReadError::UnauthenticatedReadError()
    : Exception("cannot read encrypted object without authentication") {}

} // namespace odr
