#import "ODRInternal.h"

#import <OdrCoreObjC/ODRError.h>

#include <odr/error_code.hpp>

#include <odr/exceptions.hpp>

#include <exception>
#include <istream>
#include <sstream>

NSErrorDomain const ODRErrorDomain = @"app.opendocument.OdrCore.ErrorDomain";

// Reopened as `odr` so the definitions below carry a meaningful `apple::`
// qualification — the repo convention, and `namespace odr::apple` here would
// make it redundant (-Wextra-qualification).
namespace odr {

std::string apple::to_string(NSString *string) {
  if (string == nil) {
    return {};
  }
  const NSUInteger length =
      [string lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  std::string result(length, '\0');
  NSUInteger used = 0;
  [string getBytes:result.data()
           maxLength:length
          usedLength:&used
            encoding:NSUTF8StringEncoding
             options:0
               range:NSMakeRange(0, string.length)
      remainingRange:nullptr];
  result.resize(used);
  return result;
}

NSString *apple::to_nsstring(const std::string &string) {
  return to_nsstring(std::string_view(string));
}

NSString *apple::to_nsstring(std::string_view string) {
  NSString *result = [[NSString alloc] initWithBytes:string.data()
                                              length:string.size()
                                            encoding:NSUTF8StringEncoding];
  // odrcore hands out bytes out of the document, which are not always valid
  // UTF-8; losing the string entirely is worse than replacing it
  return result != nil ? result : @"";
}

NSData *apple::to_nsdata(const std::string &bytes) {
  return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

NSData *apple::to_nsdata(std::istream &stream) {
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return to_nsdata(std::move(buffer).str());
}

namespace {

#define ODR_SAME_CODE(code, objc)                                              \
  static_assert(static_cast<int>(odr::ErrorCode::code) == objc,                \
                "ODRError must stay odr::ErrorCode numbered the same")

ODR_SAME_CODE(unknown, ODRErrorUnknown);
ODR_SAME_CODE(unsupported_operation, ODRErrorUnsupportedOperation);
ODR_SAME_CODE(file_not_found, ODRErrorFileNotFound);
ODR_SAME_CODE(unknown_file_type, ODRErrorUnknownFileType);
ODR_SAME_CODE(unsupported_file_type, ODRErrorUnsupportedFileType);
ODR_SAME_CODE(file_read_error, ODRErrorFileReadError);
ODR_SAME_CODE(file_write_error, ODRErrorFileWriteError);
ODR_SAME_CODE(no_document_file, ODRErrorNoDocumentFile);
ODR_SAME_CODE(unknown_document_type, ODRErrorUnknownDocumentType);
ODR_SAME_CODE(unsupported_crypto_algorithm, ODRErrorUnsupportedCryptoAlgorithm);
ODR_SAME_CODE(wrong_password, ODRErrorWrongPassword);
ODR_SAME_CODE(decryption_failed, ODRErrorDecryptionFailed);
ODR_SAME_CODE(not_encrypted, ODRErrorNotEncrypted);
ODR_SAME_CODE(file_encrypted, ODRErrorFileEncrypted);
ODR_SAME_CODE(document_copy_protected, ODRErrorDocumentCopyProtected);

#undef ODR_SAME_CODE

/// `ODRError` is `odr::ErrorCode` numbered the same, so this is a cast. A code
/// past the ones `ODRError` names reports `ODRErrorUnknown`.
ODRError error_code() {
  try {
    throw;
  } catch (const std::exception &e) {
    const odr::ErrorCode code = odr::error_code(e);
    return code > odr::ErrorCode::document_copy_protected
               ? ODRErrorUnknown
               : static_cast<ODRError>(code);
  } catch (...) {
    return ODRErrorUnknown;
  }
}

NSString *error_message() {
  try {
    throw;
  } catch (const std::exception &e) {
    return apple::to_nsstring(std::string(e.what()));
  } catch (...) {
    return @"unknown error";
  }
}

} // namespace

void apple::report_swallowed(const char *what) {
  NSLog(@"odr: swallowed an error the caller cannot be told about: %s", what);
}

void apple::fill_error(NSError **error) {
  const ODRError code = error_code();
  NSString *const message = error_message();
  if (error != nullptr) {
    *error = [NSError errorWithDomain:ODRErrorDomain
                                 code:code
                             userInfo:@{NSLocalizedDescriptionKey : message}];
  }
}

} // namespace odr
