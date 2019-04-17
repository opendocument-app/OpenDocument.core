#ifdef ODR_CRYPTO

#include <string>
#include "gtest/gtest.h"
#include "tinyxml2.h"
#include "cryptopp/filters.h"
#include "cryptopp/pwdbased.h"
#include "cryptopp/sha.h"
#include "cryptopp/modes.h"
#include "cryptopp/aes.h"
#include "cryptopp/zinflate.h"
#include "glog/logging.h"
#include "OpenDocumentFile.h"

TEST(DecryptionTest, open) {
    const std::string path = "../../test/encrypted.odt";
    const std::string password = "password";

    auto odf = odr::OpenDocumentFile::create();
    odf->open(path);
    odf->decrypt(password);

    for (auto &&entry : odf->getEntries()) {
        if (!entry.second.encrypted) {
            continue;
        }

        LOG(INFO) << *odf->loadText(entry.first);
    }
}

#endif
