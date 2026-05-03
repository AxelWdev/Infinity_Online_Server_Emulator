/*
 * unpack.c  -  Infinity .bin pack unpacker
 *
 * Definitive file layout (confirmed from Ghidra + raw byte analysis):
 *
 *  [0x00]                  char[4]  "raw1"  (file magic, skipped)
 *  [0x04 .. dir_offset-1]  <file data payloads, raw, no XOR>
 *  [dir_offset .. eof-4)   deflate-compressed+XOR-encrypted entry table
 *  [eof-4]                 u32 dir_offset  (XOR encrypted, abs key)
 *
 *  Fields near dir_offset (read via abs-position XOR key):
 *
 *  PRIMARY   magic (sum == 0x10203040):
 *    [dir_offset - 0x0C]  u32 word_A
 *    [dir_offset - 0x08]  u32 word_B   (A+B == 0x10203040)
 *    [dir_offset - 0x04]  u32 entry_count
 *
 *  SECONDARY magic (sum == 0x50607080):
 *    [dir_offset - 0x0E]  u32 word_A
 *    [dir_offset - 0x0A]  u32 word_B   (A+B == 0x50607080)
 *    [dir_offset - 0x06]  u16 hash
 *    [dir_offset - 0x04]  u32 dir_plain_size
 *
 *  Compressed entry table:
 *    Location: [dir_offset .. filesize-4) in the file
 *    XOR key for byte i: xkey(dir_offset + i)
 *      where xkey(p) = ((p&0x8F) ^ TABLE[p%13]) + (int8_t)(p>>7)
 *      TABLE = DAT_1004e508[0..12]
 *    After XOR decryption: raw deflate (inflateInit2, windowBits=-15)
 *
 *  Each decompressed entry record (variable length):
 *    u32  data_offset   file offset of raw payload
 *    u32  data_size     unpacked payload size
 *    u32  unk          stored payload size when fl=0x01
 *    u8   flags
 *    char name[]        null-terminated
 *
 *  File payloads at data_offset are stored raw (no XOR, no compression).
 *
 * Build:
 *   gcc -O2 -o unpack.exe unpack.c -lz
 *   (zlib: pacman -S mingw-w64-x86_64-zlib  OR  vcpkg install zlib:x64-windows)
 *
 * Usage:
 *   unpack.exe <file.bin> [--list] [-v] [output_dir]
 */

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>

#ifdef _WIN32
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#  define SEP '\\'
#else
#  include <sys/stat.h>
#  define MKDIR(p) mkdir(p,0755)
#  define SEP '/'
#endif

/* DAT_1004e508[0..12] */
static const uint8_t TABLE[13] = {
    0x42,0x8F,0xE5,0xFF,0x02,0x02,0x99,
    0x8F,0x8F,0x01,0x77,0x83,0x9E
};

static int     verbose = 0;
static FILE   *g_fp;
static long    g_fsize;

#define vlog(...) do{ if(verbose) printf(__VA_ARGS__); }while(0)

/* -------------------------------------------------------------------------
 * XOR key — exact translation of PackIO_ReadAndDecryptBytes inner loop:
 *   key = ((uint8)(pos & 0x8F) ^ TABLE[pos % 13]) + (int8_t)(pos >> 7)
 * ------------------------------------------------------------------------- */
static inline uint8_t xkey(uint32_t pos)
{
    return (uint8_t)(
        ((uint8_t)(pos & 0x8F) ^ TABLE[pos % 13])
        + (int8_t)(pos >> 7)
    );
}

/* Read + decrypt using absolute file position as XOR key */
static void xread_abs(void *buf, size_t n)
{
    long pos = ftell(g_fp);
    if (fread(buf, 1, n, g_fp) != n) {
        fprintf(stderr, "[-] Unexpected EOF at 0x%08lX\n", pos);
        exit(1);
    }
    uint8_t *p = (uint8_t *)buf;
    for (size_t i = 0; i < n; i++)
        p[i] ^= xkey((uint32_t)(pos + i));
}

static uint32_t xread_u32(void)
{ uint8_t b[4]={0}; xread_abs(b,4); return (uint32_t)(b[0]|b[1]<<8|b[2]<<16|b[3]<<24); }
static uint16_t xread_u16(void)
{ uint8_t b[2]={0}; xread_abs(b,2); return (uint16_t)(b[0]|b[1]<<8); }

/* -------------------------------------------------------------------------
 * Decrypt + decompress entry table
 *
 * Location in file: [dir_offset .. filesize-4)
 * XOR key base    : dir_offset  (= CPackedFileIO_MAGIC[4])
 * After XOR       : raw deflate (windowBits = -15)
 * ------------------------------------------------------------------------- */
static uint8_t *decrypt_decompress(uint32_t dir_offset, uint32_t comp_len,
                                    size_t *out_size)
{
    vlog("[decomp] reading compressed block at 0x%08X, len=%u, xor_base=0x%08X\n",
         dir_offset, comp_len, dir_offset);

    uint8_t *cbuf = (uint8_t *)malloc(comp_len);
    if (!cbuf) { fputs("[-] OOM cbuf\n",stderr); return NULL; }

    fseek(g_fp, (long)dir_offset, SEEK_SET);
    if (fread(cbuf, 1, comp_len, g_fp) != comp_len) {
        fputs("[-] Short read on compressed block\n",stderr);
        free(cbuf); return NULL;
    }

    /* XOR decrypt: key base = dir_offset */
    for (uint32_t i = 0; i < comp_len; i++)
        cbuf[i] ^= xkey(dir_offset + i);

    vlog("[decomp] first 8 decrypted bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
         cbuf[0],cbuf[1],cbuf[2],cbuf[3],cbuf[4],cbuf[5],cbuf[6],cbuf[7]);

    /* Raw inflate (no zlib/gzip header) */
    size_t   cap  = comp_len * 8 + 65536;
    uint8_t *ubuf = (uint8_t *)malloc(cap);
    if (!ubuf) { fputs("[-] OOM ubuf\n",stderr); free(cbuf); return NULL; }

    z_stream zs = {0};
    zs.next_in  = cbuf;
    zs.avail_in = comp_len;

    /* Raw inflate, no zlib/gzip header (original code worked for listing) */
    if (inflateInit2(&zs, -15) != Z_OK) {
        fputs("[-] inflateInit2 failed\n",stderr);
        free(cbuf); free(ubuf); return NULL;
    }

    int zr;
    do {
        if (zs.total_out + 65536 > cap) {
            cap *= 2;
            uint8_t *tmp = (uint8_t *)realloc(ubuf, cap);
            if (!tmp) {
                fputs("[-] OOM realloc\n",stderr);
                inflateEnd(&zs); free(cbuf); free(ubuf); return NULL;
            }
            ubuf = tmp;
        }
        zs.next_out  = ubuf + zs.total_out;
        zs.avail_out = (uInt)(cap - zs.total_out);
        zr = inflate(&zs, Z_NO_FLUSH);
    } while (zr == Z_OK);

    inflateEnd(&zs);
    free(cbuf);

    if (zr != Z_STREAM_END) {
        fprintf(stderr, "[-] inflate error %d: %s\n", zr, zs.msg ? zs.msg : "?");
        free(ubuf); return NULL;
    }

    *out_size = (size_t)zs.total_out;
    printf("[+] Decomp   : %zu bytes\n\n", (size_t)zs.total_out);
    return ubuf;
}

/* -------------------------------------------------------------------------
 * Entry record
 * ------------------------------------------------------------------------- */
typedef struct {
    uint32_t data_offset;  /* payload offset in file */
    uint32_t data_size;    /* unpacked size */
    uint32_t decomp_size;  /* stored size when fl=0x01, otherwise usually matches data_size */
    uint8_t  flags;        /* 0x00=raw, 0x01=XOR+deflate compressed */
    char     name[512];
} Entry;

static uint32_t entry_stored_size(const Entry *e)
{
    return e->flags == 0x01 ? e->decomp_size : e->data_size;
}

static uint32_t entry_unpacked_size(const Entry *e)
{
    return e->data_size;
}

static uint32_t buf_u32(const uint8_t *b, size_t *p)
{ uint32_t v=(uint32_t)(b[*p]|b[*p+1]<<8|b[*p+2]<<16|b[*p+3]<<24); *p+=4; return v; }
static uint8_t  buf_u8 (const uint8_t *b, size_t *p) { return b[(*p)++]; }
static void buf_cstr(const uint8_t *b, size_t blen, size_t *p, char *out, int max)
{
    int n=0;
    while(*p<blen){ uint8_t c=b[(*p)++]; if(!c||n>=max-1) break; out[n++]=(char)c; }
    out[n]='\0';
}

/* -------------------------------------------------------------------------
 * mkdir -p
 * ------------------------------------------------------------------------- */
static void mkdirs(const char *path)
{
    char tmp[2048]; snprintf(tmp,sizeof(tmp),"%s",path);
    for(char *p=tmp+1;*p;p++){
        if(*p=='/'||*p=='\\'){char c=*p;*p='\0';MKDIR(tmp);*p=c;}
    }
    MKDIR(tmp); /* also create the final component */
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
            "Usage: %s <file.bin> [--list] [-v] [output_dir]\n"
            "Build: gcc -O2 -o unpack.exe unpack.c -lz\n", argv[0]);
        return 1;
    }

    const char *infile=argv[1], *outdir="unpacked";
    int do_list=0, do_dump=0;
    for(int i=2;i<argc;i++){
        if(!strcmp(argv[i],"--list")) do_list=1;
        else if(!strcmp(argv[i],"-v")) verbose=1;
        else if(!strcmp(argv[i],"--dump")) do_dump=1;
        else if((!strcmp(argv[i],"-o")||!strcmp(argv[i],"--output")) && i+1<argc) outdir=argv[++i];
        else outdir=argv[i];
    }

    g_fp=fopen(infile,"rb");
    if(!g_fp){perror(infile);return 1;}
    fseek(g_fp,0,SEEK_END); g_fsize=ftell(g_fp);

    printf("[*] Input    : %s  (%ld bytes)\n", infile, g_fsize);
    printf("[*] XOR table: ");
    for(int i=0;i<13;i++) printf("%02X",TABLE[i]);
    printf("  (DAT_1004e508)\n");
    if(!do_list) printf("[*] Output   : %s/\n", outdir);
    printf("\n");

    /* Verify "raw1" magic */
    char magic[4];
    fseek(g_fp,0,SEEK_SET);
    fread(magic,1,4,g_fp);
    if(memcmp(magic,"raw1",4)!=0) {
        fprintf(stderr,"[!] Warning: expected 'raw1' header, got: %02X %02X %02X %02X\n",
                (uint8_t)magic[0],(uint8_t)magic[1],
                (uint8_t)magic[2],(uint8_t)magic[3]);
    } else {
        vlog("[hdr] 'raw1' magic OK\n");
    }

    /* Step 1: read dir_offset from last 4 bytes */
    fseek(g_fp, g_fsize-4, SEEK_SET);
    uint32_t dir_offset = xread_u32();
    vlog("[hdr] dir_offset=0x%08X  filesize=0x%08lX\n", dir_offset, g_fsize);

    if ((int32_t)dir_offset < 0 || dir_offset >= (uint32_t)(g_fsize-4)) {
        fprintf(stderr,"[-] dir_offset 0x%08X out of range\n", dir_offset);
        fclose(g_fp); return 1;
    }

    /* Dump raw XOR-decrypted bytes around dir_offset for diagnosis (verbose only) */
    if (verbose) {
        printf("[dbg] Bytes at file offset 0x04 (after 'raw1' magic):\n      ");
        fseek(g_fp, 4, SEEK_SET);
        for (int d = 0; d < 16; d++) {
            uint8_t b; fread(&b,1,1,g_fp);
            printf("%02X ", b);
        }
        printf("\n[dbg] Same bytes XOR-decrypted:\n      ");
        fseek(g_fp, 4, SEEK_SET);
        for (int d = 0; d < 16; d++) {
            uint8_t b; fread(&b,1,1,g_fp);
            b ^= xkey((uint32_t)(4 + d));
            printf("%02X ", b);
        }
        printf("\n\n");

        printf("[dbg] XOR-decrypted bytes at dir_offset-0x10 through dir_offset+4:\n");
        long dump_start = (long)dir_offset - 0x10;
        fseek(g_fp, dump_start, SEEK_SET);
        printf("      ");
        for (int d = 0; d < 0x14; d++) {
            uint8_t b; fread(&b,1,1,g_fp);
            b ^= xkey((uint32_t)(dump_start + d));
            printf("%02X ", b);
            if (d == 0x0F) printf("| ");
        }
        printf("\n      offset: -10 -F  -E  -D  -C  -B  -A  -9  -8  -7  -6  -5  -4  -3  -2  -1  | +0  +1  +2  +3\n\n");
    }

    /* Step 2: read magic pair + entry_count
     * PRIMARY:   seek to dir_offset-0x0C, read wa(4)+wb(4), assert sum=0x10203040, read count(4)
     * SECONDARY: seek to dir_offset-0x0E, read wa(4)+wb(4), assert sum=0x50607080,
     *            read hash(2)+count(4)                                               */
    fseek(g_fp, (long)dir_offset - 0x0C, SEEK_SET);
    uint32_t wa = xread_u32();
    uint32_t wb = xread_u32();
    vlog("[hdr] primary check: wa=0x%08X wb=0x%08X sum=0x%08X\n", wa, wb, wa+wb);

    uint32_t entry_count = 0;

    if (wa + wb == 0x10203040u) {
        printf("[+] Magic    : PRIMARY (0x10203040)\n");
        entry_count = xread_u32();
    } else {
        fseek(g_fp, (long)dir_offset - 0x0E, SEEK_SET);
        wa = xread_u32(); wb = xread_u32();
        vlog("[hdr] secondary check: wa=0x%08X wb=0x%08X sum=0x%08X\n", wa, wb, wa+wb);
        if (wa + wb != 0x50607080u) {
            fprintf(stderr, "[-] Magic not found (got primary=0x%08X secondary=0x%08X)\n",
                    wa+wb, wa+wb);
            fclose(g_fp); return 1;
        }
        printf("[+] Magic    : SECONDARY (0x50607080)\n");
        uint16_t hash = xread_u16();
        printf("[+] Hash     : 0x%04X\n", hash);
        uint32_t dir_plain_size = xread_u32(); /* directory inflate output size hint */
        vlog("[dbg] wa=0x%08X wb=0x%08X dir_plain_size=%u\n", wa, wb, dir_plain_size);
        /* entry_count is not stored in the header for SECONDARY format —
         * the decompressed block is self-delimiting: parse until buffer exhausted. */
        entry_count = 0; /* will be determined after decompression */
    }

    /* Compressed block: [dir_offset .. filesize-4) — original working value */
    uint32_t comp_len = (uint32_t)(g_fsize - 4) - dir_offset;

    printf("[+] Dir ptr  : 0x%08X\n", dir_offset);
    printf("[+] Comp len : %u bytes\n", comp_len);

    if (entry_count > 1000000u) {
        fprintf(stderr, "[-] Suspicious entry count: %u\n", entry_count);
        fclose(g_fp); return 1;
    }

    /* Step 3: decrypt + decompress */
    size_t   decomp_len = 0;
    uint8_t *dbuf = decrypt_decompress(dir_offset, comp_len, &decomp_len);
    if (!dbuf) { fclose(g_fp); return 1; }

    /* Step 4: parse entries — scan the full decompressed buffer greedily.
     * If entry_count==0 (SECONDARY format), count dynamically. */
    uint32_t max_entries = entry_count ? entry_count : 100000u;
    Entry *entries = (Entry *)calloc(max_entries, sizeof(Entry));
    if (!entries) { fputs("[-] OOM\n",stderr); free(dbuf); fclose(g_fp); return 1; }

    size_t dpos = 0;
    uint32_t n = 0;
    for (; n < max_entries && dpos + 13 <= decomp_len; n++) {
        entries[n].data_offset = buf_u32(dbuf, &dpos);
        entries[n].data_size   = buf_u32(dbuf, &dpos);
        entries[n].decomp_size = buf_u32(dbuf, &dpos);
        entries[n].flags       = buf_u8 (dbuf, &dpos);
        buf_cstr(dbuf, decomp_len, &dpos, entries[n].name, sizeof(entries[n].name));
        vlog("  [%5u] off=0x%08X unpacked=%-10u stored=%-10u fl=0x%02X  %s\n",
             n, entries[n].data_offset, entry_unpacked_size(&entries[n]),
             entry_stored_size(&entries[n]), entries[n].flags, entries[n].name);
        if (dpos >= decomp_len) { n++; break; }
    }
    entry_count = n;
    printf("[+] Entries  : %u\n", entry_count);
    free(dbuf);

    /* Step 5: list or extract */
    if (do_dump) {
        printf("First 5 entries (raw):\n");
        for (uint32_t i = 0; i < entry_count && i < 5; i++) {
            printf("  [%u] off=0x%08X unpacked=%u stored=%u fl=0x%02X name=%s\n",
                   i, entries[i].data_offset, entry_unpacked_size(&entries[i]),
                   entry_stored_size(&entries[i]),
                   entries[i].flags, entries[i].name);
            /* print first 16 bytes at data_offset raw (before XOR) */
            fseek(g_fp, (long)entries[i].data_offset, SEEK_SET);
            printf("       raw bytes at offset:");
            for(int j=0;j<16&&j<(int)entry_stored_size(&entries[i]);j++){
                uint8_t b; fread(&b,1,1,g_fp); printf(" %02X",b);
            }
            printf("\n       xor bytes at offset:");
            for(int j=0;j<16&&j<(int)entry_stored_size(&entries[i]);j++){
                uint8_t b=0;
                fseek(g_fp,(long)entries[i].data_offset+j,SEEK_SET);
                fread(&b,1,1,g_fp);
                b ^= xkey((uint32_t)(entries[i].data_offset + j));
                printf(" %02X",b);
            }
            printf("\n");
        }
    } else if (do_list) {
        printf("%-6s  %-10s  %-12s  %-12s  %-4s  %s\n","#","Offset","Stored(B)","Unpacked(B)","Fl","Name");
        printf("%.80s\n","--------------------------------------------------------------------------------");
        for (uint32_t i = 0; i < entry_count; i++)
            printf("%-6u  0x%08X  %-12u  %-12u  0x%02X  %s\n",
                   i, entries[i].data_offset, entry_stored_size(&entries[i]),
                   entry_unpacked_size(&entries[i]), entries[i].flags, entries[i].name);
        printf("\n%u entries total.\n", entry_count);
    } else {
        MKDIR(outdir);
        printf("[+] Extracting %u entries to '%s/'\n\n", entry_count, outdir);
        int w=1; {uint32_t t=entry_count; while(t>=10){t/=10;w++;}}

        for (uint32_t i = 0; i < entry_count; i++) {
            Entry *e = &entries[i];
            size_t nl = strlen(e->name);
            if (nl && (e->name[nl-1]=='/'||e->name[nl-1]=='\\')) continue;

            char out_path[2048];
            snprintf(out_path,sizeof(out_path),"%s/%s",outdir,e->name);
            for(char *p=out_path;*p;p++) if(*p=='/'||*p=='\\') *p=SEP;

            char parent[2048]; snprintf(parent,sizeof(parent),"%s",out_path);
            char *sep=strrchr(parent,SEP);
            if(sep){*sep='\0';mkdirs(parent);}

            /* Clamp read to available file bytes — some compressed payloads
             * extend into the metadata region; zlib will stop at stream end */
            uint32_t stored_sz = entry_stored_size(e);
            uint32_t avail = (e->data_offset < (uint32_t)g_fsize)
                             ? (uint32_t)g_fsize - e->data_offset : 0;
            uint32_t read_sz = (stored_sz < avail) ? stored_sz : avail;

            fseek(g_fp,(long)e->data_offset,SEEK_SET);
            uint8_t *cbuf=(uint8_t*)malloc(read_sz?read_sz:1);
            if(!cbuf){fprintf(stderr,"  [!] OOM\n");continue;}
            size_t got=fread(cbuf,1,read_sz,g_fp);

            /* XOR decrypt only for fl=0x01 (CPackedFileIO_MAGIC).
             * fl=0x00 uses CPackedFileIO (FUN_10008f20) which does plain fread, no XOR. */
            if (e->flags == 0x01) {
                for(size_t j=0;j<got;j++)
                    cbuf[j] ^= xkey((uint32_t)(e->data_offset + j));
            }

            uint8_t  *out_buf  = cbuf;
            size_t    out_size = got;
            int       short_flag = (read_sz < stored_sz);

            if (e->flags == 0x01) {
                /* fl=0x01: payload is XOR-decrypted then raw-deflate compressed.
                 * data_size field = uncompressed size hint (may be 0). */
                size_t cap = e->data_size ? (size_t)e->data_size : (size_t)got * 8 + 65536;
                uint8_t *ubuf = (uint8_t*)malloc(cap);
                if (!ubuf) { fprintf(stderr,"  [!] OOM inflate\n"); free(cbuf); continue; }

                z_stream zs = {0};
                zs.next_in  = cbuf;
                zs.avail_in = (uInt)got;

                int zr = inflateInit2(&zs, -15);
                if (zr != Z_OK) {
                    fprintf(stderr,"  [!] inflateInit2 failed\n");
                    free(ubuf); out_buf = cbuf; out_size = got;
                } else {
                    do {
                        if (zs.total_out + 65536 > cap) {
                            cap *= 2;
                            uint8_t *tmp = (uint8_t*)realloc(ubuf, cap);
                            if (!tmp) { fputs("  [!] OOM realloc\n",stderr); break; }
                            ubuf = tmp;
                        }
                        zs.next_out  = ubuf + zs.total_out;
                        zs.avail_out = (uInt)(cap - zs.total_out);
                        zr = inflate(&zs, Z_NO_FLUSH);
                    } while (zr == Z_OK);
                    inflateEnd(&zs);

                    if (zr == Z_STREAM_END) {
                        free(cbuf);
                        out_buf  = ubuf;
                        out_size = zs.total_out;
                    } else {
                        fprintf(stderr,"  [!] inflate error %d for %s\n", zr, e->name);
                        free(ubuf);
                        out_buf  = cbuf;
                        out_size = got;
                    }
                }
            }

            FILE *fo=fopen(out_path,"wb");
            if(!fo){fprintf(stderr,"  [!] Can't create '%s'\n",out_path);free(out_buf);continue;}
            fwrite(out_buf,1,out_size,fo);
            fclose(fo); free(out_buf);

            printf("  [%*u/%u]  fl=0x%02X  %10u B  %s%s\n",
                   w,i+1,entry_count,e->flags,(unsigned)out_size,e->name,
                   short_flag?"  [SHORT-clamped]":"");
        }
        printf("\n[+] Done.\n");
    }

    free(entries);
    fclose(g_fp);
    return 0;
}
