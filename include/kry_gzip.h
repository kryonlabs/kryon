/*
 * kry_gzip.h - Kry standard library: gzip container compress/decompress.
 *
 * Decompression is a complete DEFLATE (RFC 1951) inflater behind the gzip
 * (RFC 1952) wrapper, so any file gzip can produce can be read. Compression
 * emits stored (uncompressed) DEFLATE blocks inside a valid gzip container:
 * the output is larger than zlib's but every inflater accepts it.
 */
#ifndef KRYON_KRY_GZIP_H
#define KRYON_KRY_GZIP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Non-zero when data starts with the gzip magic bytes 1F 8B. */
int kry_gzip_is(const unsigned char *data, unsigned long len);

/* Inflate a gzip stream. Returns a NUL-terminated malloc'd buffer (the NUL
 * is a convenience beyond out_len for text callers) and stores the decoded
 * byte count in *out_len. Returns NULL on malformed input, bad CRC or bad
 * length. */
unsigned char *kry_gzip_decompress(const unsigned char *data,
                                   unsigned long len,
                                   unsigned long *out_len);

/* Wrap data in a valid gzip stream using stored DEFLATE blocks. Returns a
 * malloc'd buffer and stores its byte count in *out_len. Returns NULL on
 * allocation failure. The input is chunked so files above 64 KiB still
 * produce well-formed streams. */
unsigned char *kry_gzip_compress(const unsigned char *data,
                                 unsigned long len,
                                 unsigned long *out_len);

/* CRC-32 (RFC 1952 polynomial) of a buffer; exposed for tests and callers
 * that verify containers themselves. */
unsigned long kry_gzip_crc32(const unsigned char *data, unsigned long len);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_GZIP_H */
