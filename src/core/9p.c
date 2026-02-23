/*
 * 9P2000 Protocol Implementation
 * Message encoding and decoding
 */

#include "kryon.h"
#include <string.h>
#include <stdlib.h>

/*
 * Endianness conversion functions
 * 9P uses little-endian byte order
 */

uint16_t le_get16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

void le_put16(uint8_t *buf, uint16_t val)
{
    buf[0] = val & 0xff;
    buf[1] = (val >> 8) & 0xff;
}

uint32_t le_get32(const uint8_t *buf)
{
    return (uint32_t)buf[0] |
           ((uint32_t)buf[1] << 8) |
           ((uint32_t)buf[2] << 16) |
           ((uint32_t)buf[3] << 24);
}

void le_put32(uint8_t *buf, uint32_t val)
{
    buf[0] = val & 0xff;
    buf[1] = (val >> 8) & 0xff;
    buf[2] = (val >> 16) & 0xff;
    buf[3] = (val >> 24) & 0xff;
}

uint64_t le_get64(const uint8_t *buf)
{
    return (uint64_t)buf[0] |
           ((uint64_t)buf[1] << 8) |
           ((uint64_t)buf[2] << 16) |
           ((uint64_t)buf[3] << 24) |
           ((uint64_t)buf[4] << 32) |
           ((uint64_t)buf[5] << 40) |
           ((uint64_t)buf[6] << 48) |
           ((uint64_t)buf[7] << 56);
}

void le_put64(uint8_t *buf, uint64_t val)
{
    buf[0] = val & 0xff;
    buf[1] = (val >> 8) & 0xff;
    buf[2] = (val >> 16) & 0xff;
    buf[3] = (val >> 24) & 0xff;
    buf[4] = (val >> 32) & 0xff;
    buf[5] = (val >> 40) & 0xff;
    buf[6] = (val >> 48) & 0xff;
    buf[7] = (val >> 56) & 0xff;
}

/*
 * String encoding/decoding helpers
 */
static size_t p9_put_string(uint8_t *buf, const char *str)
{
    size_t len = strlen(str);
    if (len > P9_MAX_STR) {
        len = P9_MAX_STR;
    }
    le_put16(buf, (uint16_t)len);
    memcpy(buf + 2, str, len);
    return 2 + len;
}

static size_t p9_get_string(const uint8_t *buf, char *str, size_t max_len)
{
    uint16_t len = le_get16(buf);
    if (len >= max_len) {
        len = max_len - 1;
    }
    memcpy(str, buf + 2, len);
    str[len] = '\0';
    return 2 + len;
}

/*
 * Header parsing
 */
int p9_parse_header(const uint8_t *buf, size_t len, P9Hdr *hdr)
{
    if (len < 7) {
        return -1;
    }

    hdr->size = le_get32(buf);
    hdr->type = buf[4];
    hdr->tag = le_get16(buf + 5);

    return 0;
}

/*
 * Tversion: size[4] Tversion tag[2] msize[4] version[s]
 */
int p9_parse_tversion(const uint8_t *buf, size_t len, uint32_t *msize, char *version)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4 + 2) {
        return -1;
    }

    *msize = le_get32(p);
    p += 4;

    p += p9_get_string(p, version, P9_MAX_VERSION);

    return 0;
}

/*
 * Tattach: size[4] Tattach tag[2] fid[4] afid[4] uname[s] aname[s]
 */
int p9_parse_tattach(const uint8_t *buf, size_t len, uint32_t *fid, uint32_t *afid, char *uname, char *aname)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4 + 4 + 2 + 2) {
        return -1;
    }

    *fid = le_get32(p);
    p += 4;

    *afid = le_get32(p);
    p += 4;

    p += p9_get_string(p, uname, P9_MAX_STR);
    p += p9_get_string(p, aname, P9_MAX_STR);

    return 0;
}

/*
 * Twalk: size[4] Twalk tag[2] fid[4] newfid[4] nwname[2] wname[nwname][s]
 */
int p9_parse_twalk(const uint8_t *buf, size_t len, uint32_t *fid, uint32_t *newfid,
                   char *wnames[], int *nwname)
{
    const uint8_t *p = buf + 7;  /* Skip header */
    uint16_t nwname_val;
    int i;

    if (len < 7 + 4 + 4 + 2) {
        return -1;
    }

    *fid = le_get32(p);
    p += 4;

    *newfid = le_get32(p);
    p += 4;

    nwname_val = le_get16(p);
    p += 2;

    /* Limit number of walk components */
    if (nwname_val > P9_MAX_WELEM) {
        nwname_val = P9_MAX_WELEM;
    }

    /* Parse all walk components */
    for (i = 0; i < nwname_val; i++) {
        /* Allocate buffer for this component */
        wnames[i] = (char *)malloc(P9_MAX_STR);
        if (wnames[i] == NULL) {
            /* Cleanup previously allocated strings */
            int j;
            for (j = 0; j < i; j++) {
                free(wnames[j]);
            }
            return -1;
        }
        p += p9_get_string(p, wnames[i], P9_MAX_STR);
    }

    *nwname = (int)nwname_val;
    return 0;
}

/*
 * Topen: size[4] Topen tag[2] fid[4] mode[1]
 */
int p9_parse_topen(const uint8_t *buf, size_t len, uint32_t *fid, uint8_t *mode)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4 + 1) {
        return -1;
    }

    *fid = le_get32(p);
    *mode = p[4];

    return 0;
}

/*
 * Tread: size[4] Tread tag[2] fid[4] offset[8] count[4]
 */
int p9_parse_tread(const uint8_t *buf, size_t len, uint32_t *fid, uint64_t *offset, uint32_t *count)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4 + 8 + 4) {
        return -1;
    }

    *fid = le_get32(p);
    p += 4;

    *offset = le_get64(p);
    p += 8;

    *count = le_get32(p);

    return 0;
}

/*
 * Twrite: size[4] Twrite tag[2] fid[4] offset[8] count[4] data[count]
 */
int p9_parse_twrite(const uint8_t *buf, size_t len, uint32_t *fid, uint64_t *offset, const char **data, uint32_t *count)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4 + 8 + 4) {
        return -1;
    }

    *fid = le_get32(p);
    p += 4;

    *offset = le_get64(p);
    p += 8;

    *count = le_get32(p);
    p += 4;

    *data = (const char *)p;

    return 0;
}

/*
 * Tclunk: size[4] Tclunk tag[2] fid[4]
 */
int p9_parse_tclunk(const uint8_t *buf, size_t len, uint32_t *fid)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4) {
        return -1;
    }

    *fid = le_get32(p);

    return 0;
}

/*
 * Tstat: size[4] Tstat tag[2] fid[4]
 */
int p9_parse_tstat(const uint8_t *buf, size_t len, uint32_t *fid)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4) {
        return -1;
    }

    *fid = le_get32(p);

    return 0;
}

/*
 * Build message header
 */
size_t p9_build_header(uint8_t *buf, P9MsgType type, uint16_t tag, size_t payload_len)
{
    uint32_t size = 7 + payload_len;

    le_put32(buf, size);
    buf[4] = (uint8_t)type;
    le_put16(buf + 5, tag);

    return 7;
}

/*
 * Build Rversion: size[4] Rversion tag[2] msize[4] version[s]
 */
size_t p9_build_rversion(uint8_t *buf, uint16_t tag, uint32_t msize, const char *version)
{
    uint8_t *p = buf + 7;  /* Skip header space */

    le_put32(p, msize);
    p += 4;

    p += p9_put_string(p, version);

    /* Now write the header */
    p9_build_header(buf, Rversion, tag, p - (buf + 7));

    return 7 + (p - (buf + 7));
}

/*
 * Build Rerror: size[4] Rerror tag[2] ename[s]
 */
size_t p9_build_rerror(uint8_t *buf, uint16_t tag, const char *ename)
{
    uint8_t *p = buf + 7;  /* Skip header space */

    p += p9_put_string(p, ename);

    p9_build_header(buf, Rerror, tag, p - (buf + 7));

    return 7 + (p - (buf + 7));
}

/*
 * Build Rattach: size[4] Rattach tag[2] qid[13]
 */
size_t p9_build_rattach(uint8_t *buf, uint16_t tag, P9Qid *qid)
{
    uint8_t *p = buf + 7;  /* Skip header space */

    p[0] = qid->type;
    le_put32(p + 1, qid->version);
    le_put64(p + 5, qid->path);
    p += 13;

    p9_build_header(buf, Rattach, tag, 13);

    return 7 + 13;
}

/*
 * Build Rwalk: size[4] Rwalk tag[2] nwqid[2] wqid[nwqid][13]
 */
size_t p9_build_rwalk(uint8_t *buf, uint16_t tag, P9Qid *wqid, int nwqid)
{
    uint8_t *p = buf + 7;  /* Skip header space */
    int i;

    le_put16(p, (uint16_t)nwqid);
    p += 2;

    for (i = 0; i < nwqid; i++) {
        p[0] = wqid[i].type;
        le_put32(p + 1, wqid[i].version);
        le_put64(p + 5, wqid[i].path);
        p += 13;
    }

    p9_build_header(buf, Rwalk, tag, p - (buf + 7));

    return 7 + (p - (buf + 7));
}

/*
 * Build Ropen: size[4] Ropen tag[2] qid[13] iounit[4]
 */
size_t p9_build_ropen(uint8_t *buf, uint16_t tag, P9Qid *qid, uint32_t iounit)
{
    uint8_t *p = buf + 7;  /* Skip header space */

    p[0] = qid->type;
    le_put32(p + 1, qid->version);
    le_put64(p + 5, qid->path);
    p += 13;

    le_put32(p, iounit);
    p += 4;

    p9_build_header(buf, Ropen, tag, 17);

    return 7 + 17;
}

/*
 * Build Rread: size[4] Rread tag[2] count[4] data[count]
 */
size_t p9_build_rread(uint8_t *buf, uint16_t tag, const char *data, uint32_t count)
{
    uint8_t *p = buf + 7;  /* Skip header space */

    le_put32(p, count);
    p += 4;

    if (count > 0) {
        memcpy(p, data, count);
        p += count;
    }

    p9_build_header(buf, Rread, tag, 4 + count);

    return 7 + 4 + count;
}

/*
 * Build Rwrite: size[4] Rwrite tag[2] count[4]
 */
size_t p9_build_rwrite(uint8_t *buf, uint16_t tag, uint32_t count)
{
    uint8_t *p = buf + 7;  /* Skip header space */

    le_put32(p, count);
    p += 4;

    p9_build_header(buf, Rwrite, tag, 4);

    return 7 + 4;
}

/*
 * Build Rclunk: size[4] Rclunk tag[2]
 */
size_t p9_build_rclunk(uint8_t *buf, uint16_t tag)
{
    p9_build_header(buf, Rclunk, tag, 0);
    return 7;
}

/*
 * Pack stat structure into buffer (helper function)
 */
size_t p9_pack_stat(uint8_t *buf, const P9Stat *stat)
{
    uint8_t *p = buf;
    uint16_t stat_size;
    uint8_t *size_ptr;

    /* Skip size field for now */
    size_ptr = p;
    p += 2;

    /* Pack stat fields */
    le_put16(p, stat->type);
    p += 2;
    le_put32(p, stat->dev);
    p += 4;
    p[0] = stat->qid.type;
    le_put32(p + 1, stat->qid.version);
    le_put64(p + 5, stat->qid.path);
    p += 13;
    le_put32(p, stat->mode);
    p += 4;
    le_put32(p, stat->atime);
    p += 4;
    le_put32(p, stat->mtime);
    p += 4;
    le_put64(p, stat->length);
    p += 8;

    /* Pack strings */
    p += p9_put_string(p, stat->name);
    p += p9_put_string(p, stat->uid);
    p += p9_put_string(p, stat->gid);
    p += p9_put_string(p, stat->muid);

    /* Write size */
    stat_size = p - (size_ptr + 2);
    le_put16(size_ptr, stat_size);

    return p - buf;
}

/*
 * Build Rstat: size[4] Rstat tag[2] stat[n]
 */
size_t p9_build_rstat(uint8_t *buf, uint16_t tag, const P9Stat *stat)
{
    uint8_t *p = buf + 7;  /* Skip header space */
    uint8_t *stat_start;
    size_t name_len, uid_len, gid_len, muid_len;
    uint16_t stat_size;

    /* Reserve space for stat size */
    stat_start = p;
    p += 2;

    /* Encode stat fields */
    le_put16(p, stat->type);
    p += 2;
    le_put32(p, stat->dev);
    p += 4;
    p[0] = stat->qid.type;
    le_put32(p + 1, stat->qid.version);
    le_put64(p + 5, stat->qid.path);
    p += 13;
    le_put32(p, stat->mode);
    p += 4;
    le_put32(p, stat->atime);
    p += 4;
    le_put32(p, stat->mtime);
    p += 4;
    le_put64(p, stat->length);
    p += 8;

    /* Strings */
    p += p9_put_string(p, stat->name);
    p += p9_put_string(p, stat->uid);
    p += p9_put_string(p, stat->gid);
    p += p9_put_string(p, stat->muid);

    /* Write stat size */
    stat_size = p - (stat_start + 2);
    le_put16(stat_start, stat_size);

    p9_build_header(buf, Rstat, tag, p - (buf + 7));

    return 7 + (p - (buf + 7));
}
