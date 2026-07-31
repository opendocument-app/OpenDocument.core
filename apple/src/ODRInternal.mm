#import "ODRInternal.h"

#import <OdrCoreObjC/ODRError.h>

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

NSData *apple::to_nsdata(std::istream &stream) {
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  const std::string bytes = buffer.str();
  return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

namespace {

/// The code for the exception being handled. Mirrors the mapping in
/// `jni/src/odr_jni.cpp::throw_java` — keep the two in step.
ODRError error_code() {
  try {
    throw;
  } catch (const odr::UnsupportedOperation &) {
    return ODRErrorUnsupportedOperation;
  } catch (const odr::FileNotFound &) {
    return ODRErrorFileNotFound;
  } catch (const odr::UnknownFileType &) {
    return ODRErrorUnknownFileType;
  } catch (const odr::UnsupportedFileType &) {
    return ODRErrorUnsupportedFileType;
  } catch (const odr::FileReadError &) {
    return ODRErrorFileReadError;
  } catch (const odr::FileWriteError &) {
    return ODRErrorFileWriteError;
  } catch (const odr::NoDocumentFile &) {
    return ODRErrorNoDocumentFile;
  } catch (const odr::UnknownDocumentType &) {
    return ODRErrorUnknownDocumentType;
  } catch (const odr::UnsupportedCryptoAlgorithm &) {
    return ODRErrorUnsupportedCryptoAlgorithm;
  } catch (const odr::WrongPasswordError &) {
    return ODRErrorWrongPassword;
  } catch (const odr::DecryptionFailed &) {
    return ODRErrorDecryptionFailed;
  } catch (const odr::NotEncryptedError &) {
    return ODRErrorNotEncrypted;
  } catch (const odr::FileEncryptedError &) {
    return ODRErrorFileEncrypted;
  } catch (const odr::DocumentCopyProtectedException &) {
    return ODRErrorDocumentCopyProtected;
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

id apple::not_yet_bound(NSError **error, const char *what) {
  if (error != nullptr) {
    NSString *const message =
        [NSString stringWithFormat:@"%s is not bound yet", what];
    *error = [NSError errorWithDomain:ODRErrorDomain
                                 code:ODRErrorUnsupportedOperation
                             userInfo:@{NSLocalizedDescriptionKey : message}];
  }
  return nil;
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
