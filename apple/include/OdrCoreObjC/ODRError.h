#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Domain of every error this framework reports.
extern NSErrorDomain const ODRErrorDomain;

/// The head of `odr::ErrorCode`, numbered the same — `ODRInternal.mm` asserts
/// that and casts. A code past this list arrives as `ODRErrorUnknown`; the C++
/// message is always the error's `NSLocalizedDescriptionKey`, so nothing is
/// lost by not having a case here.
typedef NS_ERROR_ENUM(ODRErrorDomain, ODRError){
    ODRErrorUnknown = 1,
    ODRErrorUnsupportedOperation = 2,
    ODRErrorFileNotFound = 3,
    ODRErrorUnknownFileType = 4,
    ODRErrorUnsupportedFileType = 5,
    ODRErrorFileReadError = 6,
    ODRErrorFileWriteError = 7,
    ODRErrorNoDocumentFile = 8,
    ODRErrorUnknownDocumentType = 9,
    ODRErrorUnsupportedCryptoAlgorithm = 10,
    ODRErrorWrongPassword = 11,
    ODRErrorDecryptionFailed = 12,
    ODRErrorNotEncrypted = 13,
    ODRErrorFileEncrypted = 14,
    ODRErrorDocumentCopyProtected = 15,
};

NS_ASSUME_NONNULL_END
