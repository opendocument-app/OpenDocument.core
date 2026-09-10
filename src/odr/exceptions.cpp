#include <odr/exceptions.hpp>

#include <odr/file.hpp>
#include <odr/odr.hpp>

namespace odr {

UnsupportedOperation::UnsupportedOperation()
    : CodedException("unsupported operation") {}

UnsupportedOperation::UnsupportedOperation(const std::string &message)
    : CodedException("unsupported operation: " + message) {}

FileNotFound::FileNotFound() : CodedException("file not found") {}

FileNotFound::FileNotFound(const std::string &path)
    : CodedException("file not found: " + path) {}

UnknownFileType::UnknownFileType() : CodedException("unknown file type") {}

UnsupportedFileType::UnsupportedFileType(const FileType file_type)
    : CodedException("unsupported file type: " +
                     file_type_to_string(file_type)),
      file_type{file_type} {}

UnsupportedTextEncoding::UnsupportedTextEncoding(
    const TextEncoding text_encoding)
    : CodedException("unsupported text encoding"),
      text_encoding{text_encoding} {}

FileReadError::FileReadError() : CodedException("file read error") {}

FileWriteError::FileWriteError(const std::string &path)
    : CodedException("file write error: " + path) {}

NoZipFile::NoZipFile() : CodedException("not a zip file") {}

ZipSaveError::ZipSaveError() : CodedException("zip save error") {}

CfbError::CfbError(const std::string &desc) : CodedException(desc) {}

NoCfbFile::NoCfbFile() : CfbError("no cfb file") {}

CfbFileCorrupted::CfbFileCorrupted() : CfbError("cfb file corrupted") {}

NoTextFile::NoTextFile() : CodedException("not a text file") {}

NoCsvFile::NoCsvFile() : CodedException("not a csv file") {}

NoMarkdownFile::NoMarkdownFile() : CodedException("not a markdown file") {}

NoJsonFile::NoJsonFile() : CodedException("not a json file") {}

NoImageFile::NoImageFile() : CodedException("not an image file") {}

NoArchiveFile::NoArchiveFile() : CodedException("not an archive file") {}

NoDocumentFile::NoDocumentFile() : CodedException("not a document file") {}

NoOpenDocumentFile::NoOpenDocumentFile()
    : CodedException("not an open document file") {}

NoOfficeOpenXmlFile::NoOfficeOpenXmlFile()
    : CodedException("not an office open xml file") {}

NoPdfFile::NoPdfFile() : CodedException("not a pdf file") {}

NoFontFile::NoFontFile() : CodedException("not a font file") {}

NoLegacyMicrosoftFile::NoLegacyMicrosoftFile()
    : CodedException("not a legacy microsoft office file") {}

NoIworkFile::NoIworkFile() : CodedException("not an iwork file") {}

NoXmlFile::NoXmlFile() : CodedException("not an xml file") {}

NoSvgFile::NoSvgFile() : CodedException("not an svg file") {}

NoRtfFile::NoRtfFile() : CodedException("not an rtf file") {}

UnsupportedCryptoAlgorithm::UnsupportedCryptoAlgorithm()
    : CodedException("unsupported crypto algorithm") {}

NoSvmFile::NoSvmFile() : CodedException("not a svm file") {}

MalformedSvmFile::MalformedSvmFile() : CodedException("malformed svm file") {}

UnsupportedEndian::UnsupportedEndian() : CodedException("unsupported endian") {}

MsUnsupportedCryptoAlgorithm::MsUnsupportedCryptoAlgorithm()
    : CodedException("unsupported crypto algorithm") {}

UnknownDocumentType::UnknownDocumentType()
    : CodedException("unknown document type") {}

ValueNotStated::ValueNotStated() : CodedException("value not stated") {}

InvalidPrefix::InvalidPrefix() : CodedException("invalid prefix string") {}

InvalidPrefix::InvalidPrefix(const std::string &prefix)
    : CodedException("invalid prefix string: " + prefix) {}

DocumentCopyProtectedException::DocumentCopyProtectedException()
    : CodedException("document copy protection") {}

ResourceNotAccessible::ResourceNotAccessible()
    : CodedException("resource not accessible") {}

ResourceNotAccessible::ResourceNotAccessible(const std::string &name,
                                             const std::string &path)
    : CodedException("resource not accessible: " + name + " at " + path) {}

PrefixInUse::PrefixInUse() : CodedException("prefix in use") {}

PrefixInUse::PrefixInUse(const std::string &prefix)
    : CodedException("prefix in use: " + prefix) {}

ServerBindFailed::ServerBindFailed(const std::string &host,
                                   const std::uint32_t port)
    : CodedException("server bind failed: " + host + ":" +
                     std::to_string(port)) {}

ServerAlreadyBound::ServerAlreadyBound()
    : CodedException("server is bound already") {}

ServerNotBound::ServerNotBound() : CodedException("server is not bound") {}

UnsupportedOption::UnsupportedOption(const std::string &message)
    : CodedException("unsupported option: " + message) {}

NullPointerError::NullPointerError(const std::string &variable)
    : CodedException("null pointer error: " + variable) {}

WrongPasswordError::WrongPasswordError()
    : CodedException("wrong password error") {}

DecryptionFailed::DecryptionFailed() : CodedException("decryption failed") {}

NotEncryptedError::NotEncryptedError()
    : CodedException("not encrypted error") {}

InvalidPath::InvalidPath(const std::string &message)
    : CodedException("invalid path: " + message) {}

UnsupportedFileEncoding::UnsupportedFileEncoding(const std::string &message)
    : CodedException("unsupported file encoding: " + message) {}

FileEncryptedError::FileEncryptedError()
    : CodedException("file encrypted error") {}

UnauthenticatedReadError::UnauthenticatedReadError()
    : CodedException("cannot read encrypted object without authentication") {}

} // namespace odr

odr::ErrorCode odr::error_code(const std::exception &exception) noexcept {
  const auto *coded = dynamic_cast<const Exception *>(&exception);
  return coded == nullptr ? ErrorCode::unknown : coded->code();
}
