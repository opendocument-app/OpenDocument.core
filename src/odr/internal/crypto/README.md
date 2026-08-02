# Crypto implementation

Thin wrappers over [Crypto++](https://www.cryptopp.com/) in `crypto_util.*`,
plus one algorithm Crypto++ does not ship: Argon2id.

## Argon2id

`crypto_argon2.*` implements Argon2id per [RFC 9106], version `0x13`, without
secret or associated data, using Crypto++ for BLAKE2b. Lanes are computed
sequentially — correct for any `p`, just not in parallel.

It is used by [ODF](../odf/README.md) for LibreOffice's "wholesome" package
encryption (LibreOffice 24.8+, ODF 1.5), which writes `t=3`, `m=65536` KiB,
`p=4` lanes.

### Why hand-rolled

- Crypto++ has no Argon2, and has had no release since 8.9.0 (2023).
- The reference implementation, [P-H-C/phc-winner-argon2], is unmaintained
  since 2021 and its Makefile cannot cross-compile for Android. ConanCenter
  [declined to carry the fix][cci-pr] because upstream would never merge it, so
  depending on it meant maintaining our own Conan recipe for one function.
  Upstream PR: [#392].
- libsodium hardcodes `p=1`, so it cannot read the files above.
- Botan and OpenSSL both work, but mean a second full crypto library.

Tests cross-check against the reference implementation's published vectors.

[RFC 9106]: https://www.rfc-editor.org/rfc/rfc9106
[P-H-C/phc-winner-argon2]: https://github.com/P-H-C/phc-winner-argon2
[cci-pr]: https://github.com/conan-io/conan-center-index/pull/27800
[#392]: https://github.com/P-H-C/phc-winner-argon2/pull/392
