# Third-Party Notices

The Flint static package includes third-party static libraries built from the
repository submodules listed in `SUBMODULES.txt`.

- raylib: zlib/libpng license, see `vendor/raylib/LICENSE` in the source tree.
- liboqs: MIT license, see `vendor/liboqs/LICENSE.txt` in the source tree.
- curl/libcurl: curl license, see `vendor/curl/COPYING` in the source tree.
- cmark-gfm: BSD-style license, see `vendor/cmark-gfm/COPYING` in the source tree.

OpenSSL is not vendored into the package by default. When `flint.pc` lists
`-lssl -lcrypto`, those libraries are expected to come from the target system or
from paths supplied with `OPENSSL_ROOT_DIR`, `OPENSSL_SSL_LIBRARY`, and
`OPENSSL_CRYPTO_LIBRARY` at build time.
