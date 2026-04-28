/*
 * enc_tool — AES-256-CBC encryption tool for audio files (AMRNB / AMRWB /
 * EVS / PCMA / PCMU) plus ENCTOOL-compatible string operations.
 *
 * Build (Linux):  cd src && make
 * Outputs     :   src/enc_tool.exe
 */

#include "UEnc.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ENC_OUT_SUFFIX  ".enc"
#define DEC_OUT_SUFFIX  ".dec"

typedef enum {
    CODEC_UNKNOWN = 0,
    CODEC_AMRNB,
    CODEC_AMRWB,
    CODEC_EVS,
    CODEC_PCMA,
    CODEC_PCMU
} codec_t;

static const char *codec_name(codec_t c)
{
    switch (c) {
        case CODEC_AMRNB:   return "AMRNB";
        case CODEC_AMRWB:   return "AMRWB";
        case CODEC_EVS:     return "EVS";
        case CODEC_PCMA:    return "PCMA";
        case CODEC_PCMU:    return "PCMU";
        default:            return "UNKNOWN";
    }
}

/* --------------------------------------------------------------------- */
/*  Codec / file detection                                                */
/* --------------------------------------------------------------------- */

static int has_suffix_ci(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    if (ls < lf) return 0;
    return strcasecmp(s + ls - lf, suffix) == 0;
}

static codec_t detect_codec(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp) {
        unsigned char buf[16] = {0};
        size_t n = fread(buf, 1, sizeof(buf), fp);
        fclose(fp);

        if (n >= 9 && memcmp(buf, "#!AMR-WB\n", 9) == 0) return CODEC_AMRWB;
        if (n >= 6 && memcmp(buf, "#!AMR\n",   6) == 0) return CODEC_AMRNB;
        if (n >= 12 && memcmp(buf, "#!EVS_MC1.0\n", 12) == 0) return CODEC_EVS;
    }
    /* Headerless codecs: rely on extension. */
    if (has_suffix_ci(path, ".pcma") || has_suffix_ci(path, ".alaw") ||
        has_suffix_ci(path, ".al"))   return CODEC_PCMA;
    if (has_suffix_ci(path, ".pcmu") || has_suffix_ci(path, ".ulaw") ||
        has_suffix_ci(path, ".mulaw") || has_suffix_ci(path, ".ul"))
        return CODEC_PCMU;
    return CODEC_UNKNOWN;
}

/* Comma-separated codec filter (e.g. "amrnb,evs,pcma"). NULL/empty = accept all. */
static int codec_filter_match(const char *filter, codec_t c)
{
    if (!filter || !*filter) return 1;
    const char *name = codec_name(c);
    const char *p = filter;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        if (len > 0 && strncasecmp(p, name, len) == 0 && strlen(name) == len) {
            return 1;
        }
        if (!comma) break;
        p = comma + 1;
    }
    return 0;
}

/* --------------------------------------------------------------------- */
/*  Path helpers                                                          */
/* --------------------------------------------------------------------- */

static int is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

static int is_regular_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode);
}

static void default_encrypt_out(const char *in_path, char *out, size_t cap)
{
    snprintf(out, cap, "%s%s", in_path, ENC_OUT_SUFFIX);
}

static void default_decrypt_out(const char *in_path, char *out, size_t cap)
{
    size_t lin = strlen(in_path);
    size_t lsuf = strlen(ENC_OUT_SUFFIX);
    if (lin > lsuf && strcasecmp(in_path + lin - lsuf, ENC_OUT_SUFFIX) == 0) {
        size_t base = lin - lsuf;
        if (base + 1 > cap) base = cap - 1;
        memcpy(out, in_path, base);
        out[base] = '\0';
    } else {
        snprintf(out, cap, "%s%s", in_path, DEC_OUT_SUFFIX);
    }
}

/* --------------------------------------------------------------------- */
/*  Single-file file-mode operations                                      */
/* --------------------------------------------------------------------- */

static int op_encrypt_file(const char *key, const char *in_path,
                           const char *out_path_opt, int in_place)
{
    char out_buf[UENC_MAX_PATH];
    char tmp_buf[UENC_MAX_PATH];
    const char *out_path;

    if (in_place) {
        if (out_path_opt) {
            fprintf(stderr, "--in-place: cannot also specify outFile\n");
            return UENC_FAILURE;
        }
        snprintf(tmp_buf, sizeof(tmp_buf), "%s.enctmp", in_path);
        out_path = tmp_buf;
    } else if (out_path_opt) {
        out_path = out_path_opt;
    } else {
        default_encrypt_out(in_path, out_buf, sizeof(out_buf));
        out_path = out_buf;
    }

    if (uenc_encrypt_file(in_path, out_path, key,
                          UENC_VERSION_DEFAULT) != UENC_SUCCESS) {
        fprintf(stderr, "encrypt failed: %s\n", in_path);
        return UENC_FAILURE;
    }
    if (in_place) {
        if (rename(out_path, in_path) != 0) {
            fprintf(stderr, "rename '%s' -> '%s': %s\n",
                    out_path, in_path, strerror(errno));
            remove(out_path);
            return UENC_FAILURE;
        }
        fprintf(stdout, "encrypted in-place: %s\n", in_path);
    } else {
        fprintf(stdout, "encrypted: %s -> %s\n", in_path, out_path);
    }
    return UENC_SUCCESS;
}

static int op_decrypt_file(const char *key, const char *in_path,
                           const char *out_path_opt, int in_place)
{
    char out_buf[UENC_MAX_PATH];
    char tmp_buf[UENC_MAX_PATH];
    const char *out_path;

    if (in_place) {
        if (out_path_opt) {
            fprintf(stderr, "--in-place: cannot also specify outFile\n");
            return UENC_FAILURE;
        }
        snprintf(tmp_buf, sizeof(tmp_buf), "%s.enctmp", in_path);
        out_path = tmp_buf;
    } else if (out_path_opt) {
        out_path = out_path_opt;
    } else {
        default_decrypt_out(in_path, out_buf, sizeof(out_buf));
        out_path = out_buf;
    }

    if (uenc_decrypt_file(in_path, out_path, key) != UENC_SUCCESS) {
        fprintf(stderr, "decrypt failed: %s\n", in_path);
        return UENC_FAILURE;
    }
    if (in_place) {
        if (rename(out_path, in_path) != 0) {
            fprintf(stderr, "rename '%s' -> '%s': %s\n",
                    out_path, in_path, strerror(errno));
            remove(out_path);
            return UENC_FAILURE;
        }
        fprintf(stdout, "decrypted in-place: %s\n", in_path);
    } else {
        fprintf(stdout, "decrypted: %s -> %s\n", in_path, out_path);
    }
    return UENC_SUCCESS;
}

/* --------------------------------------------------------------------- */
/*  Batch (recursive directory) operations                                */
/* --------------------------------------------------------------------- */

typedef struct {
    int total;
    int processed;
    int skipped;
    int failed;
} batch_stats_t;

static int batch_walk(const char *root, const char *key, int encrypt_mode,
                      int in_place, const char *codec_filter,
                      batch_stats_t *st);

static int finalize_in_place(const char *tmp, const char *path)
{
    if (rename(tmp, path) != 0) {
        fprintf(stderr, "rename '%s' -> '%s': %s\n",
                tmp, path, strerror(errno));
        remove(tmp);
        return UENC_FAILURE;
    }
    return UENC_SUCCESS;
}

static int batch_handle_file(const char *path, const char *key,
                             int encrypt_mode, int in_place,
                             const char *codec_filter, batch_stats_t *st)
{
    st->total++;
    if (encrypt_mode) {
        /* Already encrypted? skip. */
        if (uenc_is_encrypted_file(path, NULL)) {
            fprintf(stdout, "skip (already encrypted): %s\n", path);
            st->skipped++;
            return UENC_SUCCESS;
        }
        codec_t c = detect_codec(path);
        if (c == CODEC_UNKNOWN) {
            fprintf(stdout, "skip (unknown codec): %s\n", path);
            st->skipped++;
            return UENC_SUCCESS;
        }
        if (!codec_filter_match(codec_filter, c)) {
            fprintf(stdout, "skip (filter): %s [%s]\n", path, codec_name(c));
            st->skipped++;
            return UENC_SUCCESS;
        }
        char out[UENC_MAX_PATH];
        if (in_place) snprintf(out, sizeof(out), "%s.enctmp", path);
        else          default_encrypt_out(path, out, sizeof(out));

        if (uenc_encrypt_file(path, out, key,
                              UENC_VERSION_DEFAULT) != UENC_SUCCESS) {
            st->failed++;
            return UENC_FAILURE;
        }
        if (in_place) {
            if (finalize_in_place(out, path) != UENC_SUCCESS) {
                st->failed++;
                return UENC_FAILURE;
            }
            fprintf(stdout, "encrypted in-place [%s]: %s\n",
                    codec_name(c), path);
        } else {
            fprintf(stdout, "encrypted [%s]: %s -> %s\n",
                    codec_name(c), path, out);
        }
        st->processed++;
        return UENC_SUCCESS;
    } else {
        if (!uenc_is_encrypted_file(path, NULL)) {
            fprintf(stdout, "skip (not encrypted): %s\n", path);
            st->skipped++;
            return UENC_SUCCESS;
        }
        char out[UENC_MAX_PATH];
        if (in_place) snprintf(out, sizeof(out), "%s.enctmp", path);
        else          default_decrypt_out(path, out, sizeof(out));

        if (uenc_decrypt_file(path, out, key) != UENC_SUCCESS) {
            st->failed++;
            return UENC_FAILURE;
        }
        if (in_place) {
            if (finalize_in_place(out, path) != UENC_SUCCESS) {
                st->failed++;
                return UENC_FAILURE;
            }
            fprintf(stdout, "decrypted in-place: %s\n", path);
        } else {
            fprintf(stdout, "decrypted: %s -> %s\n", path, out);
        }
        st->processed++;
        return UENC_SUCCESS;
    }
}

static int batch_walk(const char *root, const char *key, int encrypt_mode,
                      int in_place, const char *codec_filter,
                      batch_stats_t *st)
{
    DIR *dp = opendir(root);
    if (!dp) {
        fprintf(stderr, "opendir '%s': %s\n", root, strerror(errno));
        return UENC_FAILURE;
    }
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' ||
             (de->d_name[1] == '.' && de->d_name[2] == '\0'))) {
            continue;
        }
        /* Skip our own in-place temporaries to avoid re-processing them. */
        size_t nlen = strlen(de->d_name);
        if (nlen >= 7 && strcmp(de->d_name + nlen - 7, ".enctmp") == 0) {
            continue;
        }
        char path[UENC_MAX_PATH];
        int n = snprintf(path, sizeof(path), "%s/%s", root, de->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            fprintf(stderr, "path too long: %s/%s\n", root, de->d_name);
            st->failed++;
            continue;
        }
        if (is_directory(path)) {
            batch_walk(path, key, encrypt_mode, in_place, codec_filter, st);
        } else if (is_regular_file(path)) {
            batch_handle_file(path, key, encrypt_mode, in_place,
                              codec_filter, st);
        }
    }
    closedir(dp);
    return UENC_SUCCESS;
}

static int op_batch_one(const char *key, const char *root, int encrypt_mode,
                        int in_place, const char *codec_filter,
                        batch_stats_t *st)
{
    if (!is_directory(root)) {
        fprintf(stderr, "not a directory: %s\n", root);
        return UENC_FAILURE;
    }
    fprintf(stdout, "==> batch %s%s: %s%s%s%s\n",
            encrypt_mode ? "encrypt" : "decrypt",
            in_place ? " (in-place)" : "",
            root,
            codec_filter ? "  (filter=" : "",
            codec_filter ? codec_filter : "",
            codec_filter ? ")" : "");
    return batch_walk(root, key, encrypt_mode, in_place, codec_filter, st);
}

/* Process one or more directories. dirs[0..n_dirs-1] each get walked. */
static int op_batch(const char *key, char **dirs, int n_dirs,
                    int encrypt_mode, int in_place,
                    const char *codec_filter)
{
    batch_stats_t st = {0};
    int any_fail = 0;
    for (int i = 0; i < n_dirs; i++) {
        if (op_batch_one(key, dirs[i], encrypt_mode, in_place,
                         codec_filter, &st) != UENC_SUCCESS) {
            any_fail = 1;
        }
    }
    fprintf(stdout, "==> done. total=%d processed=%d skipped=%d failed=%d\n",
            st.total, st.processed, st.skipped, st.failed);
    return (any_fail || st.failed > 0) ? UENC_FAILURE : UENC_SUCCESS;
}

/* --------------------------------------------------------------------- */
/*  Usage / argument parsing                                              */
/* --------------------------------------------------------------------- */

static void usage(const char *prog)
{
    fprintf(stderr,
"Usage:\n"
"  String mode (ENCTOOL-compatible):\n"
"    %s -s <key>                       Save key to enc.key\n"
"    %s -l                             Load (print) saved key\n"
"    %s -e <key> <plaintext>           Encrypt string by key (base64 out)\n"
"    %s -d <key> <ciphertext>          Decrypt base64 string by key\n"
"    %s -E <plaintext>                 Encrypt string with saved key\n"
"    %s -D <ciphertext>                Decrypt string with saved key\n"
"\n"
"  File mode (audio: AMRNB/AMRWB/EVS/PCMA/PCMU):\n"
"    %s -ef <key> <inFile> [outFile]   Encrypt single file\n"
"    %s -df <key> <inFile> [outFile]   Decrypt single file\n"
"    %s -Ef <inFile> [outFile]         Encrypt file with saved key\n"
"    %s -Df <inFile> [outFile]         Decrypt file with saved key\n"
"      (add --in-place to overwrite the source instead of writing .enc)\n"
"\n"
"  Batch mode (recursive directory walk; auto codec detection):\n"
"    %s -eb <key> <dir> [<dir>...] [--codec LIST] [--in-place]\n"
"    %s -db <key> <dir> [<dir>...]                [--in-place]\n"
"    %s -Eb <dir> [<dir>...]        [--codec LIST] [--in-place]\n"
"    %s -Db <dir> [<dir>...]                       [--in-place]\n"
"\n"
"  Misc:\n"
"    %s -i <file> [<file>...]          Show codec / encryption info\n"
"    %s -h | --help                    Show this help\n"
"\n"
"  --codec LIST  comma-separated subset of {amrnb,amrwb,evs,pcma,pcmu}\n"
"  --in-place    overwrite source files instead of writing .enc/.dec siblings\n"
"  Saved key location: $%s/%s, default ./%s\n"
"  Encrypted file format: \"#!ENC1\\n\" + Salted__ + 8B salt + AES-256-CBC ciphertext\n"
"  Algorithm: AES-256-CBC, key+IV via PBKDF2-HMAC-SHA1 (%d iter)\n",
        prog, prog, prog, prog, prog, prog,
        prog, prog, prog, prog,
        prog, prog, prog, prog,
        prog, prog,
        UENC_KEY_PATH_ENV, UENC_KEY_NAME, UENC_KEY_NAME,
        UENC_PBKDF2_ITER);
}

/* Find an option like --codec or --codec=foo within argv (after parsing
 * positional args). Removes the option from positional array logic by
 * simply scanning. Returns the value or NULL. */
static const char *find_long_opt(int argc, char **argv, const char *name)
{
    size_t nlen = strlen(name);
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], name, nlen) == 0) {
            if (argv[i][nlen] == '=') return argv[i] + nlen + 1;
            if (argv[i][nlen] == '\0' && i + 1 < argc) return argv[i + 1];
        }
    }
    return NULL;
}

/* Strip a valued option like --codec[=foo] from argv. Consumes one trailing
 * argument if the value follows in the next argv slot. */
static int strip_valued_opt(int argc, char **argv, const char *name)
{
    size_t nlen = strlen(name);
    int w = 1;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], name, nlen) == 0) {
            if (argv[i][nlen] == '=') { continue; }
            if (argv[i][nlen] == '\0') { i++; continue; }
        }
        argv[w++] = argv[i];
    }
    return w;
}

/* Strip a boolean flag like --in-place from argv. */
static int strip_flag(int argc, char **argv, const char *name)
{
    int w = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], name) == 0) continue;
        argv[w++] = argv[i];
    }
    return w;
}

static int has_flag(int argc, char **argv, const char *name)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], name) == 0) return 1;
    }
    return 0;
}

static int op_info(const char *path)
{
    if (!is_regular_file(path)) {
        fprintf(stderr, "not a regular file: %s\n", path);
        return UENC_FAILURE;
    }
    int v = 0;
    if (uenc_is_encrypted_file(path, &v)) {
        struct stat st;
        long size = stat(path, &st) == 0 ? (long)st.st_size : -1;
        fprintf(stdout, "%s: enc_tool ciphertext (version=%d, size=%ld bytes)\n",
                path, v, size);
        return UENC_SUCCESS;
    }
    codec_t c = detect_codec(path);
    fprintf(stdout, "%s: %s plaintext\n", path, codec_name(c));
    return UENC_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Error: No option provided.\n");
        usage(argv[0]);
        return UENC_FAILURE;
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        usage(argv[0]);
        return UENC_SUCCESS;
    }

    /* Pre-extract --codec / --in-place before positional dispatch. */
    const char *codec_filter = find_long_opt(argc, argv, "--codec");
    argc = strip_valued_opt(argc, argv, "--codec");
    int in_place = has_flag(argc, argv, "--in-place");
    argc = strip_flag(argc, argv, "--in-place");

    const char *flag = argv[1];
    char saved_key[UENC_MAX_KEY_LEN + 1];

    /* ---- String mode (ENCTOOL-compatible) -------------------------- */
    if (strcmp(flag, "-s") == 0) {
        if (argc != 3) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_save_key(argv[2]) != UENC_SUCCESS) {
            fprintf(stderr, "save key failed!!\n");
            return UENC_FAILURE;
        }
        fprintf(stdout, "save key success!!\n");
        return UENC_SUCCESS;
    }
    if (strcmp(flag, "-l") == 0) {
        if (argc != 2) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_load_key(saved_key, sizeof(saved_key)) != UENC_SUCCESS) {
            fprintf(stderr, "load key failed!!\n");
            return UENC_FAILURE;
        }
        fprintf(stdout, "Loaded key: %s\n", saved_key);
        return UENC_SUCCESS;
    }
    if (strcmp(flag, "-e") == 0) {
        if (argc != 4) { usage(argv[0]); return UENC_FAILURE; }
        char b64[UENC_MAX_STR_B64];
        if (uenc_encrypt_str(argv[3], argv[2], b64, sizeof(b64)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        fprintf(stdout, "%s\n", b64);
        return UENC_SUCCESS;
    }
    if (strcmp(flag, "-d") == 0) {
        if (argc != 4) { usage(argv[0]); return UENC_FAILURE; }
        char plain[UENC_MAX_STR_PLAIN];
        if (uenc_decrypt_str(argv[3], argv[2], plain, sizeof(plain)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        fprintf(stdout, "%s\n", plain);
        return UENC_SUCCESS;
    }
    if (strcmp(flag, "-E") == 0) {
        if (argc != 3) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_load_key(saved_key, sizeof(saved_key)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        char b64[UENC_MAX_STR_B64];
        if (uenc_encrypt_str(argv[2], saved_key, b64, sizeof(b64)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        fprintf(stdout, "%s\n", b64);
        return UENC_SUCCESS;
    }
    if (strcmp(flag, "-D") == 0) {
        if (argc != 3) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_load_key(saved_key, sizeof(saved_key)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        char plain[UENC_MAX_STR_PLAIN];
        if (uenc_decrypt_str(argv[2], saved_key, plain, sizeof(plain)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        fprintf(stdout, "%s\n", plain);
        return UENC_SUCCESS;
    }

    /* ---- File mode -------------------------------------------------- */
    if (strcmp(flag, "-ef") == 0) {
        if (argc < 4 || argc > 5) { usage(argv[0]); return UENC_FAILURE; }
        return op_encrypt_file(argv[2], argv[3],
                               argc == 5 ? argv[4] : NULL, in_place);
    }
    if (strcmp(flag, "-df") == 0) {
        if (argc < 4 || argc > 5) { usage(argv[0]); return UENC_FAILURE; }
        return op_decrypt_file(argv[2], argv[3],
                               argc == 5 ? argv[4] : NULL, in_place);
    }
    if (strcmp(flag, "-Ef") == 0) {
        if (argc < 3 || argc > 4) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_load_key(saved_key, sizeof(saved_key)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        return op_encrypt_file(saved_key, argv[2],
                               argc == 4 ? argv[3] : NULL, in_place);
    }
    if (strcmp(flag, "-Df") == 0) {
        if (argc < 3 || argc > 4) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_load_key(saved_key, sizeof(saved_key)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        return op_decrypt_file(saved_key, argv[2],
                               argc == 4 ? argv[3] : NULL, in_place);
    }

    /* ---- Batch mode (one or more directories) ----------------------- */
    if (strcmp(flag, "-eb") == 0) {
        if (argc < 4) { usage(argv[0]); return UENC_FAILURE; }
        return op_batch(argv[2], &argv[3], argc - 3, 1, in_place, codec_filter);
    }
    if (strcmp(flag, "-db") == 0) {
        if (argc < 4) { usage(argv[0]); return UENC_FAILURE; }
        return op_batch(argv[2], &argv[3], argc - 3, 0, in_place, codec_filter);
    }
    if (strcmp(flag, "-Eb") == 0) {
        if (argc < 3) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_load_key(saved_key, sizeof(saved_key)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        return op_batch(saved_key, &argv[2], argc - 2, 1, in_place, codec_filter);
    }
    if (strcmp(flag, "-Db") == 0) {
        if (argc < 3) { usage(argv[0]); return UENC_FAILURE; }
        if (uenc_load_key(saved_key, sizeof(saved_key)) != UENC_SUCCESS) {
            return UENC_FAILURE;
        }
        return op_batch(saved_key, &argv[2], argc - 2, 0, in_place, codec_filter);
    }

    /* ---- Info ------------------------------------------------------- */
    if (strcmp(flag, "-i") == 0) {
        if (argc < 3) { usage(argv[0]); return UENC_FAILURE; }
        int rc = UENC_SUCCESS;
        for (int i = 2; i < argc; i++) {
            if (op_info(argv[i]) != UENC_SUCCESS) rc = UENC_FAILURE;
        }
        return rc;
    }

    fprintf(stderr, "Error: Unknown option '%s'.\n", flag);
    usage(argv[0]);
    return UENC_FAILURE;
}
