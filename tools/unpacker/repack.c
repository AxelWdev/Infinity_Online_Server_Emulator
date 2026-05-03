/*
 * repack.c  -  Infinity .bin repacker
 *
 * Confirmed layout (from unpack.c + base.dll pack loader):
 *
 *  [0x00]                  char[4]  "raw1"
 *  [0x04 .. hdr_offset-1]  file payloads
 *  [hdr_offset .. dir_offset-1]
 *      PRIMARY   : u32 word_A, u32 word_B, u32 entry_count
 *      SECONDARY : u32 word_A, u32 word_B, u16 hash, u32 dir_plain_size
 *      Header bytes are XOR-encrypted with the same absolute-position key as
 *      the directory and packed file payloads.
 *  [dir_offset .. eof-4)   raw-deflate directory table, XOR-encrypted
 *  [eof-4]                 u32 dir_offset, XOR-encrypted
 *
 *  Directory entry record:
 *      u32 data_offset
 *      u32 size_a
 *      u32 size_b
 *      u8  flags        (0x00 = raw, 0x01 = XOR + raw-deflate)
 *      char name[]      null-terminated
 *
 *  On the live data packs under test:
 *      fl=0x01 => size_a = uncompressed size, size_b = stored compressed size
 *
 *  SECONDARY hash check (confirmed from base.dll FUN_10009730):
 *      hash = rotate_right_16(sum_chars(lowercase_ascii(basename)), 4) ... ^ 0x8764
 *
 * Usage:
 *   repack.exe <template.bin> <input_dir> <output.bin> [options]
 *   repack.exe --overlay <template.bin> <input_dir> <output.bin> [options] <relative_path...>
 *   repack.exe --append  <template.bin> <input_dir> <output.bin> [options] <relative_path...>
 *
 * Options:
 *   -v                 verbose logging
 *   --hash-name <name> use this basename for SECONDARY header hash
 *   --level <0-9>      zlib level for rebuilt streams (default: 6)
 *
 * Notes:
 *   - Entry order, names, flags, and header magic words are preserved from the
 *     template archive.
 *   - Unchanged entries reuse the template payload bytes. For flag 0x01 entries
 *     those bytes are re-keyed to the new absolute offset so the stream stays
 *     valid even when earlier file sizes changed.
 *   - Modified flag 0x01 entries are rebuilt as raw-deflate + XOR, matching the
 *     scheme the client expects.
 *   - Overlay mode builds a minimal pack from explicit relative paths, using the
 *     template archive only for header mode / magic words.
 *   - Append mode keeps the template archive entries and appends explicit extra
 *     relative paths at the end of the pack.
 */

#define _CRT_SECURE_NO_WARNINGS
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#ifdef _WIN32
#  include <direct.h>
#  define PATH_SEP '\\'
#  define FSEEK _fseeki64
#  define FTELL _ftelli64
typedef __int64 fileoff_t;
#else
#  include <sys/stat.h>
#  define PATH_SEP '/'
#  define FSEEK fseeko
#  define FTELL ftello
typedef off_t fileoff_t;
#endif

static const uint8_t TABLE[13] = {
    0x42, 0x8F, 0xE5, 0xFF, 0x02, 0x02, 0x99,
    0x8F, 0x8F, 0x01, 0x77, 0x83, 0x9E
};

enum {
    HEADER_PRIMARY = 1,
    HEADER_SECONDARY = 2
};

typedef struct {
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t decomp_size;
    uint8_t flags;
    char *name;
} Entry;

typedef struct {
    FILE *fp;
    char *path;
    fileoff_t file_size;
    uint32_t dir_offset;
    uint32_t word_a;
    uint32_t word_b;
    uint16_t header_hash;
    uint32_t header_value;
    uint32_t entry_count;
    int header_mode;
    Entry *entries;
} TemplatePack;

static int verbose = 0;

#define vlog(...) do { if (verbose) printf(__VA_ARGS__); } while (0)

static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) die("[-] Out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q) die("[-] Out of memory");
    return q;
}

static char *xstrdup_local(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = (char *)xmalloc(n);
    memcpy(copy, s, n);
    return copy;
}

static inline uint8_t xkey(uint32_t pos)
{
    return (uint8_t)((((uint8_t)(pos & 0x8F)) ^ TABLE[pos % 13]) + (int8_t)(pos >> 7));
}

static void xcrypt_buffer(uint8_t *buf, size_t len, uint32_t base_pos)
{
    size_t i;
    for (i = 0; i < len; ++i)
        buf[i] ^= xkey(base_pos + (uint32_t)i);
}

static void write_le16(uint8_t *dst, uint16_t v)
{
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void write_le32(uint8_t *dst, uint32_t v)
{
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
    dst[2] = (uint8_t)((v >> 16) & 0xFF);
    dst[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t read_le32(const uint8_t *src)
{
    return (uint32_t)src[0]
        | ((uint32_t)src[1] << 8)
        | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[3] << 24);
}

static uint16_t read_le16(const uint8_t *src)
{
    return (uint16_t)(src[0] | (src[1] << 8));
}

static void file_seek(FILE *fp, fileoff_t offset, int whence, const char *what)
{
    if (FSEEK(fp, offset, whence) != 0)
        die("[-] Seek failed while %s", what);
}

static void read_exact(FILE *fp, void *buf, size_t n, const char *what)
{
    if (n && fread(buf, 1, n, fp) != n)
        die("[-] Short read while %s", what);
}

static void write_exact(FILE *fp, const void *buf, size_t n, const char *what)
{
    if (n && fwrite(buf, 1, n, fp) != n)
        die("[-] Short write while %s", what);
}

static void xread_abs(FILE *fp, void *buf, size_t n)
{
    fileoff_t pos = FTELL(fp);
    uint8_t *p = (uint8_t *)buf;
    size_t i;
    read_exact(fp, buf, n, "reading encrypted bytes");
    for (i = 0; i < n; ++i)
        p[i] ^= xkey((uint32_t)(pos + (fileoff_t)i));
}

static uint32_t xread_u32(FILE *fp)
{
    uint8_t b[4];
    xread_abs(fp, b, sizeof(b));
    return read_le32(b);
}

static uint16_t xread_u16(FILE *fp)
{
    uint8_t b[2];
    xread_abs(fp, b, sizeof(b));
    return read_le16(b);
}

static const char *basename_only(const char *path)
{
    const char *p = path;
    const char *last = path;
    while (*p) {
        if (*p == '/' || *p == '\\')
            last = p + 1;
        ++p;
    }
    return last;
}

static uint16_t compute_secondary_hash(const char *path_or_name)
{
    const unsigned char *p = (const unsigned char *)basename_only(path_or_name);
    uint16_t acc = 0;

    while (*p) {
        uint16_t ch = *p++;
        uint16_t sum;
        if (ch >= 'A' && ch <= 'Z')
            ch = (uint16_t)(ch + 0x20);
        sum = (uint16_t)(acc + ch);
        acc = (uint16_t)(((sum >> 4) | (sum << 12)) & 0xFFFFu);
    }

    return (uint16_t)(acc ^ 0x8764u);
}

static uint8_t *inflate_raw_buffer(const uint8_t *src, size_t src_len,
                                   size_t expected_size, size_t *out_len)
{
    z_stream zs;
    uint8_t *out;
    size_t cap;
    int zr;

    memset(&zs, 0, sizeof(zs));
    cap = expected_size ? expected_size : (src_len ? src_len * 8 + 65536 : 1024);
    out = (uint8_t *)xmalloc(cap);

    zs.next_in = (Bytef *)src;
    zs.avail_in = (uInt)src_len;

    if (inflateInit2(&zs, -15) != Z_OK) {
        free(out);
        die("[-] inflateInit2 failed");
    }

    do {
        if (zs.total_out == cap) {
            cap *= 2;
            out = (uint8_t *)xrealloc(out, cap);
        }
        zs.next_out = out + zs.total_out;
        zs.avail_out = (uInt)(cap - zs.total_out);
        zr = inflate(&zs, Z_NO_FLUSH);
    } while (zr == Z_OK);

    inflateEnd(&zs);

    if (zr != Z_STREAM_END) {
        free(out);
        die("[-] inflate error %d (%s)", zr, zs.msg ? zs.msg : "?");
    }

    *out_len = (size_t)zs.total_out;
    return out;
}

static uint8_t *deflate_raw_buffer(const uint8_t *src, size_t src_len,
                                   int level, size_t *out_len)
{
    z_stream zs;
    uint8_t *out;
    size_t cap;
    int zr;

    memset(&zs, 0, sizeof(zs));
    cap = compressBound((uLong)(src_len ? src_len : 1)) + 64;
    out = (uint8_t *)xmalloc(cap);

    zs.next_in = (Bytef *)(src_len ? src : (const uint8_t *)"");
    zs.avail_in = (uInt)src_len;

    if (deflateInit2(&zs, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        free(out);
        die("[-] deflateInit2 failed");
    }

    do {
        if (zs.total_out == cap) {
            cap *= 2;
            out = (uint8_t *)xrealloc(out, cap);
        }
        zs.next_out = out + zs.total_out;
        zs.avail_out = (uInt)(cap - zs.total_out);
        zr = deflate(&zs, Z_FINISH);
    } while (zr == Z_OK);

    deflateEnd(&zs);

    if (zr != Z_STREAM_END) {
        free(out);
        die("[-] deflate error %d", zr);
    }

    *out_len = (size_t)zs.total_out;
    return out;
}

static int path_join(char *dst, size_t dst_cap, const char *base, const char *rel)
{
    size_t i, n = 0;

    if (!dst_cap)
        return 0;

    if (base && *base) {
        n = strlen(base);
        if (n + 1 >= dst_cap)
            return 0;
        memcpy(dst, base, n);
        if (dst[n - 1] != '/' && dst[n - 1] != '\\')
            dst[n++] = PATH_SEP;
    }

    for (i = 0; rel[i]; ++i) {
        char c = rel[i];
        if (n + 1 >= dst_cap)
            return 0;
        if (c == '/' || c == '\\')
            c = PATH_SEP;
        dst[n++] = c;
    }

    dst[n] = '\0';
    return 1;
}

static char *normalize_pack_path(const char *path)
{
    size_t i;
    size_t len;
    size_t start = 0;
    char *out;

    while (path[start] == '/' || path[start] == '\\')
        ++start;

    len = strlen(path + start);
    out = (char *)xmalloc(len + 1);
    memcpy(out, path + start, len + 1);

    for (i = 0; i < len; ++i) {
        if (out[i] == '\\')
            out[i] = '/';
    }

    return out;
}

static uint32_t entry_stored_size(const Entry *e)
{
    if (e->flags == 0x01)
        return e->decomp_size;
    return e->data_size;
}

static uint32_t entry_unpacked_size(const Entry *e)
{
    return e->data_size;
}

static int load_file_if_exists(const char *path, uint8_t **out_buf, size_t *out_size)
{
    FILE *fp = fopen(path, "rb");
    fileoff_t size;
    uint8_t *buf;

    *out_buf = NULL;
    *out_size = 0;

    if (!fp)
        return 0;

    if (FSEEK(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        die("[-] Failed to seek '%s'", path);
    }

    size = FTELL(fp);
    if (size < 0) {
        fclose(fp);
        die("[-] Failed to size '%s'", path);
    }
    if ((uint64_t)size > SIZE_MAX) {
        fclose(fp);
        die("[-] File too large for this build: %s", path);
    }

    if (FSEEK(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        die("[-] Failed to rewind '%s'", path);
    }

    buf = (uint8_t *)xmalloc((size_t)size ? (size_t)size : 1);
    read_exact(fp, buf, (size_t)size, path);
    fclose(fp);

    *out_buf = buf;
    *out_size = (size_t)size;
    return 1;
}

static void copy_file_verbatim(const char *src_path, const char *dst_path)
{
    FILE *in_fp = fopen(src_path, "rb");
    FILE *out_fp;
    uint8_t buffer[1 << 16];
    size_t got;

    if (!in_fp)
        die("[-] Can't reopen '%s' for copying: %s", src_path, strerror(errno));

    out_fp = fopen(dst_path, "wb");
    if (!out_fp) {
        fclose(in_fp);
        die("[-] Can't create '%s': %s", dst_path, strerror(errno));
    }

    while ((got = fread(buffer, 1, sizeof(buffer), in_fp)) != 0)
        write_exact(out_fp, buffer, got, "copying unchanged archive");

    fclose(out_fp);
    fclose(in_fp);
}

static uint8_t *read_template_raw_bytes(const TemplatePack *pack, const Entry *e)
{
    uint32_t stored_size = entry_stored_size(e);
    uint8_t *buf = (uint8_t *)xmalloc(stored_size ? stored_size : 1);
    file_seek(pack->fp, (fileoff_t)e->data_offset, SEEK_SET, "reading payload");
    read_exact(pack->fp, buf, stored_size, e->name);
    return buf;
}

static uint8_t *read_template_decoded_bytes(const TemplatePack *pack, const Entry *e, size_t *out_size)
{
    uint8_t *raw;

    if (e->flags == 0x00) {
        raw = read_template_raw_bytes(pack, e);
        *out_size = entry_unpacked_size(e);
        return raw;
    }

    if (e->flags != 0x01)
        die("[-] Unsupported entry flag 0x%02X for '%s'", e->flags, e->name);

    raw = read_template_raw_bytes(pack, e);
    xcrypt_buffer(raw, entry_stored_size(e), e->data_offset);

    {
        uint8_t *decoded = inflate_raw_buffer(raw, entry_stored_size(e), entry_unpacked_size(e), out_size);
        free(raw);
        return decoded;
    }
}

static int entry_is_directory(const Entry *e)
{
    size_t len = strlen(e->name);
    if (!len)
        return 0;
    return e->name[len - 1] == '/' || e->name[len - 1] == '\\';
}

static int entry_name_exists(const Entry *entries, uint32_t count, const char *name)
{
    uint32_t i;
    for (i = 0; i < count; ++i) {
        if (strcmp(entries[i].name, name) == 0)
            return 1;
    }
    return 0;
}

static int disk_matches_template(const TemplatePack *pack, const Entry *e,
                                 const uint8_t *disk_buf, size_t disk_size)
{
    uint8_t *template_buf;
    size_t template_size;
    int same;

    if (e->flags == 0x00 && disk_size != entry_unpacked_size(e))
        return 0;
    if (e->flags == 0x01 && disk_size != entry_unpacked_size(e))
        return 0;
    if (e->flags != 0x00 && e->flags != 0x01)
        die("[-] Unsupported entry flag 0x%02X for '%s'", e->flags, e->name);

    template_buf = read_template_decoded_bytes(pack, e, &template_size);
    same = (template_size == disk_size)
        && (disk_size == 0 || memcmp(template_buf, disk_buf, disk_size) == 0);
    free(template_buf);
    return same;
}

static void free_entries(Entry *entries, uint32_t count)
{
    uint32_t i;
    if (!entries)
        return;
    for (i = 0; i < count; ++i)
        free(entries[i].name);
    free(entries);
}

static void close_template(TemplatePack *pack)
{
    if (!pack)
        return;
    if (pack->fp)
        fclose(pack->fp);
    free(pack->path);
    free_entries(pack->entries, pack->entry_count);
    memset(pack, 0, sizeof(*pack));
}

static void parse_template(const char *path, TemplatePack *out_pack)
{
    FILE *fp = fopen(path, "rb");
    fileoff_t fsize;
    char magic[4];
    uint32_t comp_len;
    uint32_t expected_primary_count = 0;
    uint8_t *comp_buf = NULL;
    uint8_t *dir_buf = NULL;
    size_t dir_len = 0;
    size_t pos = 0;
    uint32_t capacity = 0;
    TemplatePack pack;

    memset(&pack, 0, sizeof(pack));

    if (!fp)
        die("[-] Can't open template '%s': %s", path, strerror(errno));

    file_seek(fp, 0, SEEK_END, "sizing template");
    fsize = FTELL(fp);
    if (fsize < 8)
        die("[-] Template too small");
    if ((uint64_t)fsize > 0xFFFFFFFFULL)
        die("[-] Template exceeds 4 GiB pack format limit");

    file_seek(fp, 0, SEEK_SET, "reading template magic");
    read_exact(fp, magic, sizeof(magic), "reading template magic");
    if (memcmp(magic, "raw1", 4) != 0)
        die("[-] Template '%s' does not start with 'raw1'", path);

    file_seek(fp, fsize - 4, SEEK_SET, "reading dir_offset");
    pack.dir_offset = xread_u32(fp);
    pack.fp = fp;
    pack.file_size = fsize;
    pack.path = xstrdup_local(path);

    if (pack.dir_offset < 4 || pack.dir_offset >= (uint32_t)(fsize - 4))
        die("[-] dir_offset 0x%08X is out of range", pack.dir_offset);

    file_seek(fp, (fileoff_t)pack.dir_offset - 12, SEEK_SET, "reading primary header");
    pack.word_a = xread_u32(fp);
    pack.word_b = xread_u32(fp);
    if (pack.word_a + pack.word_b == 0x10203040u) {
        pack.header_mode = HEADER_PRIMARY;
        pack.header_value = xread_u32(fp);
        expected_primary_count = pack.header_value;
    } else {
        file_seek(fp, (fileoff_t)pack.dir_offset - 14, SEEK_SET, "reading secondary header");
        pack.word_a = xread_u32(fp);
        pack.word_b = xread_u32(fp);
        if (pack.word_a + pack.word_b != 0x50607080u)
            die("[-] Pack header magic not recognized");
        pack.header_mode = HEADER_SECONDARY;
        pack.header_hash = xread_u16(fp);
        pack.header_value = xread_u32(fp);
    }

    if (expected_primary_count > 1000000u)
        die("[-] Suspicious entry count: %u", expected_primary_count);

    if (pack.header_mode == HEADER_SECONDARY) {
        uint16_t expected = compute_secondary_hash(path);
        if (expected != pack.header_hash) {
            vlog("[!] Secondary hash mismatch for template basename: file=0x%04X expected=0x%04X\n",
                 pack.header_hash, expected);
        }
        vlog("[hdr] secondary dir plain size hint: %u bytes\n", pack.header_value);
    }

    comp_len = (uint32_t)(fsize - 4 - pack.dir_offset);
    comp_buf = (uint8_t *)xmalloc(comp_len ? comp_len : 1);
    file_seek(fp, pack.dir_offset, SEEK_SET, "reading directory blob");
    read_exact(fp, comp_buf, comp_len, "reading directory blob");
    xcrypt_buffer(comp_buf, comp_len, pack.dir_offset);
    dir_buf = inflate_raw_buffer(comp_buf, comp_len, pack.header_mode == HEADER_SECONDARY ? pack.header_value : expected_primary_count * 32u, &dir_len);
    free(comp_buf);
    comp_buf = NULL;

    while (pos + 13 <= dir_len) {
        size_t name_len;
        Entry *e;

        if (pack.entry_count == capacity) {
            capacity = capacity ? capacity * 2u : 32u;
            pack.entries = (Entry *)xrealloc(pack.entries, capacity * sizeof(Entry));
        }

        e = &pack.entries[pack.entry_count];
        memset(e, 0, sizeof(*e));

        e->data_offset = read_le32(dir_buf + pos); pos += 4;
        e->data_size = read_le32(dir_buf + pos); pos += 4;
        e->decomp_size = read_le32(dir_buf + pos); pos += 4;
        e->flags = dir_buf[pos++];

        name_len = 0;
        while (pos + name_len < dir_len && dir_buf[pos + name_len] != '\0')
            ++name_len;
        if (pos + name_len >= dir_len)
            die("[-] Unterminated entry name for entry %u", pack.entry_count);

        e->name = (char *)xmalloc(name_len + 1);
        memcpy(e->name, dir_buf + pos, name_len);
        e->name[name_len] = '\0';
        pos += name_len + 1;
        ++pack.entry_count;
    }

    if (pos != dir_len)
        die("[-] Directory table has %zu trailing bytes after entry parsing", dir_len - pos);

    if (pack.header_mode == HEADER_PRIMARY && expected_primary_count != pack.entry_count) {
        die("[-] PRIMARY header count mismatch: header=%u parsed=%u",
            expected_primary_count, pack.entry_count);
    }

    free(dir_buf);
    *out_pack = pack;
}

static uint8_t *build_directory_table(const Entry *entries, uint32_t entry_count, size_t *out_len)
{
    uint8_t *buf;
    size_t total = 0;
    size_t pos = 0;
    uint32_t i;

    for (i = 0; i < entry_count; ++i)
        total += 13 + strlen(entries[i].name) + 1;

    buf = (uint8_t *)xmalloc(total ? total : 1);

    for (i = 0; i < entry_count; ++i) {
        size_t name_len = strlen(entries[i].name) + 1;
        write_le32(buf + pos, entries[i].data_offset); pos += 4;
        write_le32(buf + pos, entries[i].data_size); pos += 4;
        write_le32(buf + pos, entries[i].decomp_size); pos += 4;
        buf[pos++] = entries[i].flags;
        memcpy(buf + pos, entries[i].name, name_len);
        pos += name_len;
    }

    *out_len = total;
    return buf;
}

static void write_rekeyed_or_raw_copy(FILE *out_fp, const TemplatePack *pack,
                                      const Entry *src, uint32_t dst_offset)
{
    uint32_t stored_size = entry_stored_size(src);
    uint8_t *buf = read_template_raw_bytes(pack, src);

    if (src->flags == 0x01 && stored_size) {
        xcrypt_buffer(buf, stored_size, src->data_offset);
        xcrypt_buffer(buf, stored_size, dst_offset);
    } else if (src->flags != 0x00 && src->flags != 0x01) {
        if (dst_offset != src->data_offset) {
            free(buf);
            die("[-] Can't safely move unsupported flag 0x%02X entry '%s'",
                src->flags, src->name);
        }
    }

    write_exact(out_fp, buf, stored_size, src->name);
    free(buf);
}

static void write_archive(FILE *out_fp,
                          const TemplatePack *pack,
                          const Entry *entries,
                          uint32_t entry_count,
                          uint32_t payload_cursor,
                          uint16_t out_hash,
                          uint32_t *out_dir_offset,
                          size_t *out_dir_plain_len,
                          size_t *out_dir_comp_len,
                          int level)
{
    uint32_t header_size = (pack->header_mode == HEADER_PRIMARY) ? 12u : 14u;
    uint32_t dir_offset;
    uint32_t tail_offset;
    uint8_t *dir_plain;
    uint8_t *dir_comp;
    size_t dir_plain_len;
    size_t dir_comp_len;

    if ((uint64_t)payload_cursor + (uint64_t)header_size > 0xFFFFFFFFULL)
        die("[-] Repacked archive exceeds the 4 GiB pack format limit");

    dir_offset = payload_cursor + header_size;
    dir_plain = build_directory_table(entries, entry_count, &dir_plain_len);
    dir_comp = deflate_raw_buffer(dir_plain, dir_plain_len, level, &dir_comp_len);
    if (dir_comp_len > 0xFFFFFFFFu) {
        free(dir_plain);
        free(dir_comp);
        die("[-] Directory table exceeds the 4 GiB pack format limit");
    }
    xcrypt_buffer(dir_comp, dir_comp_len, dir_offset);

    {
        uint8_t header_buf[14];
        memset(header_buf, 0, sizeof(header_buf));
        write_le32(header_buf + 0, pack->word_a);
        write_le32(header_buf + 4, pack->word_b);
        if (pack->header_mode == HEADER_PRIMARY) {
            write_le32(header_buf + 8, entry_count);
        } else {
            write_le16(header_buf + 8, out_hash);
            write_le32(header_buf + 10, (uint32_t)dir_plain_len);
        }
        xcrypt_buffer(header_buf, header_size, payload_cursor);
        write_exact(out_fp, header_buf, header_size, "writing header");
    }

    write_exact(out_fp, dir_comp, dir_comp_len, "writing directory table");

    if ((uint64_t)dir_offset + (uint64_t)dir_comp_len + 4ull > 0xFFFFFFFFULL) {
        free(dir_plain);
        free(dir_comp);
        die("[-] Repacked archive exceeds the 4 GiB pack format limit");
    }

    tail_offset = dir_offset + (uint32_t)dir_comp_len;
    {
        uint8_t tail[4];
        write_le32(tail, dir_offset);
        xcrypt_buffer(tail, sizeof(tail), tail_offset);
        write_exact(out_fp, tail, sizeof(tail), "writing directory pointer");
    }

    if (out_dir_offset)
        *out_dir_offset = dir_offset;
    if (out_dir_plain_len)
        *out_dir_plain_len = dir_plain_len;
    if (out_dir_comp_len)
        *out_dir_comp_len = dir_comp_len;

    free(dir_plain);
    free(dir_comp);
}

int main(int argc, char **argv)
{
    int overlay_mode = 0;
    int append_mode = 0;
    const char *template_path;
    const char *input_dir;
    const char *output_path;
    const char *hash_name_override = NULL;
    TemplatePack pack;
    Entry *new_entries = NULL;
    FILE *out_fp = NULL;
    char **extra_paths = NULL;
    uint32_t extra_count = 0;
    uint32_t extra_cap = 0;
    uint32_t i;
    uint32_t arg_index;
    uint32_t payload_cursor = 4;
    uint32_t reused_count = 0;
    uint32_t rebuilt_count = 0;
    uint32_t missing_count = 0;
    uint32_t appended_count = 0;
    uint32_t dir_offset = 0;
    uint16_t out_hash = 0;
    size_t dir_plain_len = 0;
    size_t dir_comp_len = 0;
    int level = 6;

    memset(&pack, 0, sizeof(pack));

    if (argc >= 2 && strcmp(argv[1], "--overlay") == 0)
        overlay_mode = 1;
    else if (argc >= 2 && strcmp(argv[1], "--append") == 0)
        append_mode = 1;

    if ((!overlay_mode && !append_mode && argc < 4) || ((overlay_mode || append_mode) && argc < 6)) {
        fprintf(stderr,
            "Usage: %s <template.bin> <input_dir> <output.bin> [options]\n"
            "       %s --overlay <template.bin> <input_dir> <output.bin> [options] <relative_path...>\n"
            "       %s --append  <template.bin> <input_dir> <output.bin> [options] <relative_path...>\n"
            "Options:\n"
            "  -v                 verbose logging\n"
            "  --hash-name <name> override SECONDARY filename hash basename\n"
            "  --level <0-9>      zlib level for rebuilt streams (default: 6)\n",
            argv[0], argv[0], argv[0]);
        return 1;
    }

    if (overlay_mode || append_mode) {
        template_path = argv[2];
        input_dir = argv[3];
        output_path = argv[4];
        arg_index = 5;
    } else {
        template_path = argv[1];
        input_dir = argv[2];
        output_path = argv[3];
        arg_index = 4;
    }

    for (i = arg_index; i < (uint32_t)argc; ++i) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--hash-name") == 0 && i + 1 < (uint32_t)argc) {
            hash_name_override = argv[++i];
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < (uint32_t)argc) {
            level = atoi(argv[++i]);
            if (level < 0 || level > 9)
                die("[-] Compression level must be between 0 and 9");
        } else if (overlay_mode || append_mode) {
            if (extra_count == extra_cap) {
                extra_cap = extra_cap ? extra_cap * 2u : 8u;
                extra_paths = (char **)xrealloc(extra_paths, extra_cap * sizeof(*extra_paths));
            }
            extra_paths[extra_count++] = argv[i];
        } else {
            die("[-] Unknown option: %s", argv[i]);
        }
    }

    parse_template(template_path, &pack);

    if (overlay_mode) {
        if (extra_count == 0)
            die("[-] Overlay mode requires at least one relative path");

        if (pack.header_mode == HEADER_SECONDARY)
            out_hash = compute_secondary_hash(hash_name_override ? hash_name_override : output_path);

        printf("[*] Template : %s\n", template_path);
        printf("[*] Input dir : %s\n", input_dir);
        printf("[*] Output    : %s\n", output_path);
        printf("[*] Mode      : overlay\n");
        printf("[*] Header    : %s\n", pack.header_mode == HEADER_PRIMARY ? "PRIMARY" : "SECONDARY");
        printf("[*] Level     : %d\n", level);
        printf("[*] Files     : %u\n", extra_count);
        if (pack.header_mode == HEADER_SECONDARY) {
            printf("[*] Hash name : %s (computed 0x%04X)\n",
                   basename_only(hash_name_override ? hash_name_override : output_path), out_hash);
        }
        printf("\n");

        new_entries = (Entry *)calloc(extra_count ? extra_count : 1, sizeof(Entry));
        if (!new_entries)
            die("[-] Out of memory");

        out_fp = fopen(output_path, "wb");
        if (!out_fp)
            die("[-] Can't create '%s': %s", output_path, strerror(errno));

        write_exact(out_fp, "raw1", 4, "writing magic");

        for (i = 0; i < extra_count; ++i) {
            char disk_path[4096];
            uint8_t *disk_buf = NULL;
            uint8_t *comp_buf = NULL;
            size_t disk_size = 0;
            size_t comp_size = 0;
            Entry *dst = &new_entries[i];

            dst->name = normalize_pack_path(extra_paths[i]);
            if (entry_name_exists(new_entries, i, dst->name))
                die("[-] Duplicate overlay path '%s'", dst->name);

            if (!path_join(disk_path, sizeof(disk_path), input_dir, extra_paths[i]))
                die("[-] Path too long for overlay entry '%s'", extra_paths[i]);
            if (!load_file_if_exists(disk_path, &disk_buf, &disk_size))
                die("[-] Missing overlay source file '%s'", disk_path);
            if (disk_size > 0xFFFFFFFFu)
                die("[-] '%s' exceeds the 4 GiB pack format limit", dst->name);

            comp_buf = deflate_raw_buffer(disk_buf, disk_size, level, &comp_size);
            if (comp_size > 0xFFFFFFFFu)
                die("[-] Compressed '%s' exceeds the 4 GiB pack format limit", dst->name);

            dst->flags = 0x01;
            dst->data_offset = payload_cursor;
            dst->data_size = (uint32_t)disk_size;
            dst->decomp_size = (uint32_t)comp_size;

            xcrypt_buffer(comp_buf, comp_size, dst->data_offset);
            write_exact(out_fp, comp_buf, comp_size, dst->name);

            if ((uint64_t)payload_cursor + (uint64_t)dst->decomp_size > 0xFFFFFFFFULL)
                die("[-] Overlay payload area exceeds the 4 GiB pack format limit");
            payload_cursor += dst->decomp_size;

            vlog("[ovr ] fl=0x%02X off=0x%08X unpacked=%u stored=%u  %s\n",
                 dst->flags, dst->data_offset, dst->data_size, dst->decomp_size, dst->name);

            free(comp_buf);
            free(disk_buf);
        }

        write_archive(out_fp, &pack, new_entries, extra_count, payload_cursor,
                      out_hash, &dir_offset, &dir_plain_len, &dir_comp_len, level);

        fclose(out_fp);
        out_fp = NULL;

        printf("[+] Created  : %u\n", extra_count);
        printf("[+] Dir ptr  : 0x%08X\n", dir_offset);
        printf("[+] Dir raw  : %zu bytes\n", dir_plain_len);
        printf("[+] Dir comp : %zu bytes\n", dir_comp_len);
        printf("\n[+] Done.\n");

        free(extra_paths);
        free_entries(new_entries, extra_count);
        close_template(&pack);
        return 0;
    }

    if (append_mode && extra_count == 0)
        die("[-] Append mode requires at least one relative path");

    printf("[*] Template : %s\n", template_path);
    printf("[*] Input dir : %s\n", input_dir);
    printf("[*] Output    : %s\n", output_path);
    if (append_mode) {
        printf("[*] Mode      : append\n");
        printf("[*] Entries   : %u + %u extra\n", pack.entry_count, extra_count);
    } else {
        printf("[*] Entries   : %u\n", pack.entry_count);
    }
    printf("[*] Header    : %s\n", pack.header_mode == HEADER_PRIMARY ? "PRIMARY" : "SECONDARY");
    printf("[*] Level     : %d\n", level);
    if (pack.header_mode == HEADER_SECONDARY) {
        if (hash_name_override) {
            out_hash = compute_secondary_hash(hash_name_override);
            printf("[*] Hash name : %s (computed 0x%04X)\n", basename_only(hash_name_override), out_hash);
        } else {
            out_hash = pack.header_hash;
            printf("[*] Hash     : preserving template value 0x%04X\n", out_hash);
        }
    }
    printf("\n");

    new_entries = (Entry *)calloc((pack.entry_count + extra_count) ? (pack.entry_count + extra_count) : 1, sizeof(Entry));
    if (!new_entries)
        die("[-] Out of memory");

    for (i = 0; i < pack.entry_count; ++i) {
        new_entries[i] = pack.entries[i];
        new_entries[i].name = xstrdup_local(pack.entries[i].name);
    }

    out_fp = fopen(output_path, "wb");
    if (!out_fp)
        die("[-] Can't create '%s': %s", output_path, strerror(errno));

    write_exact(out_fp, "raw1", 4, "writing magic");

    for (i = 0; i < pack.entry_count; ++i) {
        Entry *src = &pack.entries[i];
        Entry *dst = &new_entries[i];
        char disk_path[4096];
        uint8_t *disk_buf = NULL;
        size_t disk_size = 0;
        int has_disk_file = 0;
        int modified = 0;

        dst->data_offset = payload_cursor;

        if (entry_is_directory(src)) {
            dst->data_size = 0;
            ++reused_count;
            vlog("[dir] %s\n", src->name);
            continue;
        }

        if (!path_join(disk_path, sizeof(disk_path), input_dir, src->name))
            die("[-] Path too long for entry '%s'", src->name);

        has_disk_file = load_file_if_exists(disk_path, &disk_buf, &disk_size);
        if (!has_disk_file) {
            ++missing_count;
            vlog("[orig] %s (missing from input dir, keeping template payload)\n", src->name);
        } else {
            modified = !disk_matches_template(&pack, src, disk_buf, disk_size);
        }

        if (!modified) {
            write_rekeyed_or_raw_copy(out_fp, &pack, src, dst->data_offset);
            dst->data_size = src->data_size;
            dst->decomp_size = src->decomp_size;
            ++reused_count;
        } else {
            if (disk_size > 0xFFFFFFFFu) {
                free(disk_buf);
                die("[-] '%s' exceeds the 4 GiB pack format limit", src->name);
            }
            if (src->flags == 0x00) {
                write_exact(out_fp, disk_buf, disk_size, src->name);
                dst->data_size = (uint32_t)disk_size;
                dst->decomp_size = (uint32_t)disk_size;
            } else if (src->flags == 0x01) {
                size_t comp_size = 0;
                uint8_t *comp_buf = deflate_raw_buffer(disk_buf, disk_size, level, &comp_size);
                if (comp_size > 0xFFFFFFFFu) {
                    free(comp_buf);
                    free(disk_buf);
                    die("[-] Compressed '%s' exceeds the 4 GiB pack format limit", src->name);
                }
                xcrypt_buffer(comp_buf, comp_size, dst->data_offset);
                write_exact(out_fp, comp_buf, comp_size, src->name);
                dst->data_size = (uint32_t)disk_size;
                dst->decomp_size = (uint32_t)comp_size;
                free(comp_buf);
            } else {
                free(disk_buf);
                die("[-] Can't rebuild unsupported flag 0x%02X entry '%s'",
                    src->flags, src->name);
            }

            ++rebuilt_count;
            vlog("[new ] fl=0x%02X off=0x%08X unpacked=%u stored=%u  %s\n",
                 dst->flags, dst->data_offset, dst->data_size, dst->decomp_size, dst->name);
        }

        if ((uint64_t)payload_cursor + (uint64_t)entry_stored_size(dst) > 0xFFFFFFFFULL) {
            free(disk_buf);
            die("[-] Repacked payload area exceeds the 4 GiB pack format limit");
        }
        payload_cursor += entry_stored_size(dst);
        free(disk_buf);
    }

    if (!append_mode && rebuilt_count == 0 && (pack.header_mode != HEADER_SECONDARY || out_hash == pack.header_hash)) {
        fclose(out_fp);
        out_fp = NULL;
        copy_file_verbatim(template_path, output_path);
        printf("[+] Reused   : %u\n", reused_count);
        printf("[+] Rebuilt  : %u\n", rebuilt_count);
        printf("[+] Missing  : %u\n", missing_count);
        printf("[+] Output   : identical copy of template (no content changes)\n");
        printf("\n[+] Done.\n");
        free_entries(new_entries, pack.entry_count);
        close_template(&pack);
        free(extra_paths);
        return 0;
    }

    if (append_mode) {
        for (i = 0; i < extra_count; ++i) {
            char disk_path[4096];
            uint8_t *disk_buf = NULL;
            uint8_t *comp_buf = NULL;
            size_t disk_size = 0;
            size_t comp_size = 0;
            Entry *dst = &new_entries[pack.entry_count + i];

            dst->name = normalize_pack_path(extra_paths[i]);
            if (entry_name_exists(new_entries, pack.entry_count + i, dst->name))
                die("[-] Append path '%s' already exists in template; modify it in-place instead", dst->name);

            if (!path_join(disk_path, sizeof(disk_path), input_dir, extra_paths[i]))
                die("[-] Path too long for append entry '%s'", extra_paths[i]);
            if (!load_file_if_exists(disk_path, &disk_buf, &disk_size))
                die("[-] Missing append source file '%s'", disk_path);
            if (disk_size > 0xFFFFFFFFu)
                die("[-] '%s' exceeds the 4 GiB pack format limit", dst->name);

            comp_buf = deflate_raw_buffer(disk_buf, disk_size, level, &comp_size);
            if (comp_size > 0xFFFFFFFFu)
                die("[-] Compressed '%s' exceeds the 4 GiB pack format limit", dst->name);

            dst->flags = 0x01;
            dst->data_offset = payload_cursor;
            dst->data_size = (uint32_t)disk_size;
            dst->decomp_size = (uint32_t)comp_size;

            xcrypt_buffer(comp_buf, comp_size, dst->data_offset);
            write_exact(out_fp, comp_buf, comp_size, dst->name);

            if ((uint64_t)payload_cursor + (uint64_t)dst->decomp_size > 0xFFFFFFFFULL)
                die("[-] Appended payload area exceeds the 4 GiB pack format limit");
            payload_cursor += dst->decomp_size;
            ++appended_count;

            vlog("[add ] fl=0x%02X off=0x%08X unpacked=%u stored=%u  %s\n",
                 dst->flags, dst->data_offset, dst->data_size, dst->decomp_size, dst->name);

            free(comp_buf);
            free(disk_buf);
        }
    }

    write_archive(out_fp, &pack, new_entries, pack.entry_count + appended_count, payload_cursor,
                  out_hash, &dir_offset, &dir_plain_len, &dir_comp_len, level);

    fclose(out_fp);
    out_fp = NULL;

    printf("[+] Reused   : %u\n", reused_count);
    printf("[+] Rebuilt  : %u\n", rebuilt_count);
    printf("[+] Missing  : %u\n", missing_count);
    if (append_mode)
        printf("[+] Added    : %u\n", appended_count);
    printf("[+] Dir ptr  : 0x%08X\n", dir_offset);
    printf("[+] Dir raw  : %zu bytes\n", dir_plain_len);
    printf("[+] Dir comp : %zu bytes\n", dir_comp_len);
    printf("\n[+] Done.\n");

    free(extra_paths);
    free_entries(new_entries, pack.entry_count + appended_count);
    close_template(&pack);
    return 0;
}
