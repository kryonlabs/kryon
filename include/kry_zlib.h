/*
 * kry_zlib.h - Kry standard library: zlib container compress/decompress.
 *
 * Decompression reuses the kry_gzip DEFLATE inflater behind the zlib
 * (RFC 1950) wrapper: CMF/FLG header validation, any window size up to
 * 32 KiB, Adler-32 verification. Compression emits stored (uncompressed)
 * DEFLATE blocks inside a valid zlib container: the output is larger than
 * zlib's but every inflater accepts it.
 */
#ifndef KRYON_KRY_ZLIB_H
#define KRYON_KRY_ZLIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* Non-zero when data starts with a usable zlib header: CM = deflate,
 * CINFO <= 7, FCHECK passes, no preset dictionary. */
int kry_zlib_is(const unsigned char *data, unsigned long len);

/* Inflate a zlib stream. Returns a NUL-terminated malloc'd buffer (the NUL
 * is a convenience beyond out_len for text callers) and stores the decoded
 * byte count in *out_len. Returns NULL on malformed input or bad Adler-32. */
unsigned char *kry_zlib_decompress(const unsigned char *data,
                                   unsigned long len,
                                   unsigned long *out_len);

/* Wrap data in a valid zlib stream using stored DEFLATE blocks. Returns a
 * malloc'd buffer and stores its byte count in *out_len. Returns NULL on
 * allocation failure. The input is chunked so payloads above 64 KiB still
 * produce well-formed streams. */
unsigned char *kry_zlib_compress(const unsigned char *data,
                                 unsigned long len,
                                 unsigned long *out_len);

/* Adler-32 (RFC 1950) of a buffer; exposed for tests and callers that
 * verify containers themselves. */
unsigned long kry_zlib_adler32(const unsigned char *data, unsigned long len);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_ZLIB_H */
