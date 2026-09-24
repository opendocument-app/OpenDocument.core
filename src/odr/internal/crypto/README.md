# Crypto implementation

Thin wrappers over [Crypto++](https://www.cryptopp.com/) in `crypto_util.*`,
plus one algorithm Crypto++ does not ship: Argon2id.

## Argon2id

`crypto_argon2.*` implements Argon2id per [RFC 9106], version `0x13`, without
secret or associated data. It uses Crypto++ for BLAKE2b. Lanes are computed
sequentially, so any `p` is correct but not parallel.

[ODF](../odf/README.md) uses it for LibreOffice's package encryption
(LibreOffice 24.8+, ODF 1.5), which writes `t=3`, `m=65536` KiB and `p=4`.

Why it is own code:

- Crypto++ has no Argon2.
- The reference implementation, [P-H-C/phc-winner-argon2], does not
  cross-compile for Android, and ConanCenter [declined the fix][cci-pr].
- libsodium hardcodes `p=1`, so it cannot read the files above.
- Botan and OpenSSL would be a second full crypto library.

`crypto_util_test.cpp` checks the output against the reference
implementation's published vectors.

[RFC 9106]: https://www.rfc-editor.org/rfc/rfc9106
[P-H-C/phc-winner-argon2]: https://github.com/P-H-C/phc-winner-argon2
[cci-pr]: https://github.com/conan-io/conan-center-index/pull/27800
