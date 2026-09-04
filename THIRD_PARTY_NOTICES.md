# Third-Party Notices

The Kryon static package includes third-party code built from the repository
submodules listed in `SUBMODULES.txt`.

- raylib: zlib/libpng license, see `vendor/raylib/LICENSE` in the source tree.
- liboqs: MIT license, see `vendor/liboqs/LICENSE.txt` in the source tree.
- curl/libcurl: curl license, see `vendor/curl/COPYING` in the source tree.
- cmark-gfm: BSD-style license, see `vendor/cmark-gfm/COPYING` in the source tree.
- Noto fonts: SIL Open Font License 1.1, see `fonts/noto/LICENSE.txt` in the source tree.
- MingCute Core Filled icons: Apache License 2.0, see
  `licenses/mingcute/LICENSE.txt` in the source tree. Kryon embeds only the
  selected, renamed monochrome subset used by its UI in `icons/ui.png`.
  Apache 2.0 does not grant rights to third-party trademarks depicted by some
  brand icons; applications remain responsible for appropriate trademark use.

OpenSSL is not vendored into the package by default. When `kryon.pc` lists
`-lssl -lcrypto`, those libraries are expected to come from the target system or
from paths supplied with `OPENSSL_ROOT_DIR`, `OPENSSL_SSL_LIBRARY`, and
`OPENSSL_CRYPTO_LIBRARY` at build time.
