#include "OpenDocumentCrypto.h"
#include "../io/Storage.h"
#include "../io/StreamUtil.h"
#include "../io/StorageUtil.h"
#include "../crypto/CryptoUtil.h"

namespace odr {

bool OpenDocumentCrypto::canDecrypt(const OpenDocumentMeta::Manifest::Entry &entry) {
    return entry.checksumType != OpenDocumentMeta::ChecksumType::UNKNOWN
           && entry.algorithm != OpenDocumentMeta::AlgorithmType::UNKNOWN
           && entry.keyDerivation != OpenDocumentMeta::KeyDerivationType::UNKNOWN
           && entry.startKeyGeneration != OpenDocumentMeta::ChecksumType::UNKNOWN;
}

std::string OpenDocumentCrypto::hash(const std::string &input, const OpenDocumentMeta::ChecksumType checksumType) {
    switch (checksumType) {
        case OpenDocumentMeta::ChecksumType::SHA256:
            return CryptoUtil::sha256(input);
        case OpenDocumentMeta::ChecksumType::SHA1:
            return CryptoUtil::sha1(input);
        case OpenDocumentMeta::ChecksumType::SHA256_1K:
            return CryptoUtil::sha256(input.substr(0, 1024));
        case OpenDocumentMeta::ChecksumType::SHA1_1K:
            return CryptoUtil::sha1(input.substr(0, 1024));
        default:
            throw std::invalid_argument("checksumType");
    }
}

std::string OpenDocumentCrypto::decrypt(const std::string &input, const std::string &derivedKey,
        const std::string &initialisationVector, const OpenDocumentMeta::AlgorithmType algorithm) {
    switch (algorithm) {
        case OpenDocumentMeta::AlgorithmType::AES256_CBC:
            return CryptoUtil::decryptAES(derivedKey, initialisationVector, input);
        case OpenDocumentMeta::AlgorithmType::TRIPLE_DES_CBC:
            return CryptoUtil::decryptTripleDES(derivedKey, initialisationVector, input);
        case OpenDocumentMeta::AlgorithmType::BLOWFISH_CFB:
            return CryptoUtil::decryptBlowfish(derivedKey, initialisationVector, input);
        default:
            throw std::invalid_argument("algorithm");
    }
}

std::string OpenDocumentCrypto::startKey(const OpenDocumentMeta::Manifest::Entry &entry, const std::string &password) {
    const std::string result = hash(password, entry.startKeyGeneration);
    if (result.size() < entry.startKeySize) throw std::invalid_argument("hash too small");
    return result.substr(0, entry.startKeySize);
}

std::string OpenDocumentCrypto::deriveKeyAndDecrypt(const OpenDocumentMeta::Manifest::Entry &entry, const std::string &startKey, const std::string &input) {
    const std::string derivedKey = CryptoUtil::pbkdf2(entry.keySize, startKey, entry.keySalt, entry.keyIterationCount);
    return decrypt(input, derivedKey, entry.initialisationVector, entry.algorithm);
}

bool OpenDocumentCrypto::validatePassword(const OpenDocumentMeta::Manifest::Entry &entry, std::string decrypted) {
    const std::size_t padding = CryptoUtil::padding(decrypted);
    decrypted = decrypted.substr(0, decrypted.size() - padding);
    const std::string checksum = hash(decrypted, entry.checksumType);
    return checksum == entry.checksum;
}

namespace {
class CryptoOpenDocumentFile : public ReadStorage {
public:
    const std::unique_ptr<Storage> parent;
    const OpenDocumentMeta::Manifest manifest;
    const std::string startKey;

    CryptoOpenDocumentFile(std::unique_ptr<Storage> parent, OpenDocumentMeta::Manifest manifest, std::string startKey) :
            parent(std::move(parent)),
            manifest(std::move(manifest)),
            startKey(std::move(startKey)) {
    }

    bool isSomething(const Path &p) const final { return parent->isSomething(p); }
    bool isFile(const Path &p) const final { return parent->isSomething(p); }
    bool isFolder(const Path &p) const final { return parent->isSomething(p); }
    bool isReadable(const Path &p) const final { return isFile(p); }

    std::uint64_t size(const Path &p) const final {
        const auto it = manifest.entries.find(p);
        if (it == manifest.entries.end()) return parent->size(p);
        return it->second.size;
    }

    void visit(const Path &p, Visiter v) const final { parent->visit(p, v); }

    std::unique_ptr<Source> read(const Path &path) const final {
        const auto it = manifest.entries.find(path);
        if (it == manifest.entries.end()) return parent->read(path);
        if (!OpenDocumentCrypto::canDecrypt(it->second)) throw UnsupportedCryptoAlgorithmException();
        std::unique_ptr<Source> source = parent->read(path);
        const std::string input = StreamUtil::read(*source);
        std::string result = CryptoUtil::inflate(OpenDocumentCrypto::deriveKeyAndDecrypt(it->second, startKey, input));
        return std::make_unique<StringSource>(std::move(result));
    }
};
}

bool OpenDocumentCrypto::decrypt(std::unique_ptr<Storage> &storage, const OpenDocumentMeta::Manifest &manifest,
        const std::string &password) {
    if (!manifest.encryted) return true;
    if (!canDecrypt(*manifest.smallestFileEntry)) throw UnsupportedCryptoAlgorithmException();
    const std::string startKey = OpenDocumentCrypto::startKey(*manifest.smallestFileEntry, password);
    const std::string input = StorageUtil::read(*storage, *manifest.smallestFilePath);
    const std::string decrypt = deriveKeyAndDecrypt(*manifest.smallestFileEntry, startKey, input);
    if (!validatePassword(*manifest.smallestFileEntry, decrypt)) return false;
    storage = std::make_unique<CryptoOpenDocumentFile>(std::move(storage), manifest, startKey);
    return true;
}

}
