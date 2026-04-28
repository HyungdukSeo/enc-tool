#include "UEnc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

/* ========================================================================
 *  Internal helpers
 * ====================================================================== */

static void log_openssl_error(const char *where)
{
    unsigned long e = ERR_get_error();
    if (e) {
        char buf[256];
        ERR_error_string_n(e, buf, sizeof(buf));
        fprintf(stderr, "[UEnc] OpenSSL error in %s: %s\n", where, buf);
    } else {
        fprintf(stderr, "[UEnc] error in %s\n", where);
    }
}

static int derive_key_iv(const char *password, const unsigned char *salt,
                         unsigned char *key, unsigned char *iv)
{
    unsigned char material[UENC_KEY_LEN + UENC_IV_LEN];

    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password),
                          salt, UENC_SALT_LEN,
                          UENC_PBKDF2_ITER, EVP_sha1(),
                          sizeof(material), material) != 1) {
        log_openssl_error("PKCS5_PBKDF2_HMAC");
        return UENC_FAILURE;
    }
    memcpy(key, material, UENC_KEY_LEN);
    memcpy(iv,  material + UENC_KEY_LEN, UENC_IV_LEN);
    OPENSSL_cleanse(material, sizeof(material));
    return UENC_SUCCESS;
}

/* AES-256-CBC encrypt a single in-memory buffer. *out_len receives output
 * length. out must be at least in_len + EVP_MAX_BLOCK_LENGTH bytes. */
static int aes_encrypt_buf(const unsigned char *in, int in_len,
                           const unsigned char *key, const unsigned char *iv,
                           unsigned char *out, int *out_len)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    int ok = 0;

    if (!ctx) { log_openssl_error("EVP_CIPHER_CTX_new"); return UENC_FAILURE; }
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        log_openssl_error("EVP_EncryptInit_ex"); goto done;
    }
    if (EVP_EncryptUpdate(ctx, out, &len, in, in_len) != 1) {
        log_openssl_error("EVP_EncryptUpdate"); goto done;
    }
    total = len;
    if (EVP_EncryptFinal_ex(ctx, out + total, &len) != 1) {
        log_openssl_error("EVP_EncryptFinal_ex"); goto done;
    }
    total += len;
    *out_len = total;
    ok = 1;
done:
    EVP_CIPHER_CTX_free(ctx);
    return ok ? UENC_SUCCESS : UENC_FAILURE;
}

static int aes_decrypt_buf(const unsigned char *in, int in_len,
                           const unsigned char *key, const unsigned char *iv,
                           unsigned char *out, int *out_len)
{
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, total = 0;
    int ok = 0;

    if (!ctx) { log_openssl_error("EVP_CIPHER_CTX_new"); return UENC_FAILURE; }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        log_openssl_error("EVP_DecryptInit_ex"); goto done;
    }
    if (EVP_DecryptUpdate(ctx, out, &len, in, in_len) != 1) {
        log_openssl_error("EVP_DecryptUpdate"); goto done;
    }
    total = len;
    if (EVP_DecryptFinal_ex(ctx, out + total, &len) != 1) {
        fprintf(stderr, "[UEnc] decrypt final failed (wrong key or corrupted data)\n");
        goto done;
    }
    total += len;
    *out_len = total;
    ok = 1;
done:
    EVP_CIPHER_CTX_free(ctx);
    return ok ? UENC_SUCCESS : UENC_FAILURE;
}

/* base64 encode without newlines. Returns bytes written (excluding NUL),
 * or -1 on failure. */
static int b64_encode(const unsigned char *in, int in_len,
                      char *out, size_t out_capacity)
{
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    BUF_MEM *bptr = NULL;
    int written = -1;

    if (!b64 || !mem) goto done;
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, mem);
    if (BIO_write(b64, in, in_len) != in_len) goto done;
    if (BIO_flush(b64) != 1) goto done;
    BIO_get_mem_ptr(b64, &bptr);
    if (!bptr) goto done;
    if (bptr->length + 1 > out_capacity) {
        fprintf(stderr, "[UEnc] b64 output overflow (%zu > %zu)\n",
                bptr->length + 1, out_capacity);
        goto done;
    }
    memcpy(out, bptr->data, bptr->length);
    out[bptr->length] = '\0';
    written = (int)bptr->length;
done:
    if (b64) BIO_free_all(b64);
    else if (mem) BIO_free(mem);
    return written;
}

/* base64 decode (NL-tolerant). Returns bytes written, or -1 on failure. */
static int b64_decode(const char *in, unsigned char *out, size_t out_capacity)
{
    int in_len = (int)strlen(in);
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new_mem_buf((void *)in, in_len);
    int n = -1;

    if (!b64 || !mem) {
        if (b64) BIO_free(b64);
        if (mem) BIO_free(mem);
        return -1;
    }
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, mem);
    n = BIO_read(b64, out, (int)out_capacity);
    BIO_free_all(b64);
    if (n < 0) return -1;
    return n;
}

/* ========================================================================
 *  Audio-file API (#!ENC<v>\n + Salted__ + salt + ciphertext)
 * ====================================================================== */

static int write_file_header(FILE *out, int version, const unsigned char *salt)
{
    unsigned char hdr[UENC_HEADER_LEN];
    size_t pos = 0;

    /* Line 1: "#!ENC<v>\n" */
    memcpy(hdr + pos, UENC_MAGIC, UENC_MAGIC_LEN); pos += UENC_MAGIC_LEN;
    hdr[pos++] = (unsigned char)('0' + version);
    hdr[pos++] = '\n';
    /* Line 2 begins: 8 random salt bytes (then ciphertext follows) */
    memcpy(hdr + pos, salt, UENC_SALT_LEN); pos += UENC_SALT_LEN;

    if (fwrite(hdr, 1, pos, out) != pos) {
        fprintf(stderr, "[UEnc] write header failed: %s\n", strerror(errno));
        return UENC_FAILURE;
    }
    return UENC_SUCCESS;
}

static int read_file_header(FILE *in, int *version_out, unsigned char *salt_out)
{
    unsigned char hdr[UENC_HEADER_LEN];
    if (fread(hdr, 1, UENC_HEADER_LEN, in) != UENC_HEADER_LEN) {
        fprintf(stderr, "[UEnc] not an encrypted file (header too short)\n");
        return UENC_FAILURE;
    }
    if (memcmp(hdr, UENC_MAGIC, UENC_MAGIC_LEN) != 0) {
        fprintf(stderr, "[UEnc] not an encrypted file (bad magic)\n");
        return UENC_FAILURE;
    }
    int v = hdr[UENC_MAGIC_LEN] - '0';
    if (v < UENC_VERSION_MIN || v > UENC_VERSION_MAX) {
        fprintf(stderr, "[UEnc] unsupported version byte 0x%02x\n",
                hdr[UENC_MAGIC_LEN]);
        return UENC_FAILURE;
    }
    if (hdr[UENC_MAGIC_LEN + 1] != '\n') {
        fprintf(stderr, "[UEnc] malformed header (missing newline)\n");
        return UENC_FAILURE;
    }
    /* Salt is the first 8 bytes of line 2 */
    memcpy(salt_out, hdr + UENC_LINE1_LEN, UENC_SALT_LEN);
    if (version_out) *version_out = v;
    return UENC_SUCCESS;
}

int uenc_encrypt_file(const char *in_path, const char *out_path,
                      const char *password, int version)
{
    if (!in_path || !out_path || !password) return UENC_FAILURE;
    if (version < UENC_VERSION_MIN || version > UENC_VERSION_MAX) {
        fprintf(stderr, "[UEnc] invalid version %d\n", version);
        return UENC_FAILURE;
    }
    if (version != 1) {
        fprintf(stderr, "[UEnc] version %d not implemented yet\n", version);
        return UENC_FAILURE;
    }

    FILE *in  = fopen(in_path,  "rb");
    if (!in) {
        fprintf(stderr, "[UEnc] open in '%s': %s\n", in_path, strerror(errno));
        return UENC_FAILURE;
    }
    FILE *out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "[UEnc] open out '%s': %s\n", out_path, strerror(errno));
        fclose(in);
        return UENC_FAILURE;
    }

    int rc = UENC_FAILURE;
    EVP_CIPHER_CTX *ctx = NULL;
    unsigned char salt[UENC_SALT_LEN];
    unsigned char key[UENC_KEY_LEN];
    unsigned char iv[UENC_IV_LEN];
    unsigned char inbuf[UENC_BUFFER_SIZE];
    unsigned char outbuf[UENC_BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH];
    int outlen = 0;

    if (RAND_bytes(salt, UENC_SALT_LEN) != 1) {
        log_openssl_error("RAND_bytes");
        goto cleanup;
    }
    if (derive_key_iv(password, salt, key, iv) != UENC_SUCCESS) goto cleanup;
    if (write_file_header(out, version, salt) != UENC_SUCCESS)   goto cleanup;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { log_openssl_error("EVP_CIPHER_CTX_new"); goto cleanup; }
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        log_openssl_error("EVP_EncryptInit_ex"); goto cleanup;
    }

    for (;;) {
        size_t n = fread(inbuf, 1, sizeof(inbuf), in);
        if (n > 0) {
            if (EVP_EncryptUpdate(ctx, outbuf, &outlen, inbuf, (int)n) != 1) {
                log_openssl_error("EVP_EncryptUpdate"); goto cleanup;
            }
            if (outlen > 0 && fwrite(outbuf, 1, (size_t)outlen, out) != (size_t)outlen) {
                fprintf(stderr, "[UEnc] write ciphertext: %s\n", strerror(errno));
                goto cleanup;
            }
        }
        if (n < sizeof(inbuf)) {
            if (ferror(in)) {
                fprintf(stderr, "[UEnc] read '%s': %s\n", in_path, strerror(errno));
                goto cleanup;
            }
            break;
        }
    }

    if (EVP_EncryptFinal_ex(ctx, outbuf, &outlen) != 1) {
        log_openssl_error("EVP_EncryptFinal_ex"); goto cleanup;
    }
    if (outlen > 0 && fwrite(outbuf, 1, (size_t)outlen, out) != (size_t)outlen) {
        fprintf(stderr, "[UEnc] write final block: %s\n", strerror(errno));
        goto cleanup;
    }

    if (fflush(out) != 0) {
        fprintf(stderr, "[UEnc] flush '%s': %s\n", out_path, strerror(errno));
        goto cleanup;
    }
    rc = UENC_SUCCESS;

cleanup:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(iv,  sizeof(iv));
    fclose(in);
    fclose(out);
    if (rc != UENC_SUCCESS) remove(out_path);
    return rc;
}

int uenc_decrypt_file(const char *in_path, const char *out_path,
                      const char *password)
{
    if (!in_path || !out_path || !password) return UENC_FAILURE;

    FILE *in = fopen(in_path, "rb");
    if (!in) {
        fprintf(stderr, "[UEnc] open in '%s': %s\n", in_path, strerror(errno));
        return UENC_FAILURE;
    }

    int version = 0;
    unsigned char salt[UENC_SALT_LEN];
    if (read_file_header(in, &version, salt) != UENC_SUCCESS) {
        fclose(in);
        return UENC_FAILURE;
    }
    if (version != 1) {
        fprintf(stderr, "[UEnc] version %d not implemented yet\n", version);
        fclose(in);
        return UENC_FAILURE;
    }

    FILE *out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "[UEnc] open out '%s': %s\n", out_path, strerror(errno));
        fclose(in);
        return UENC_FAILURE;
    }

    int rc = UENC_FAILURE;
    EVP_CIPHER_CTX *ctx = NULL;
    unsigned char key[UENC_KEY_LEN];
    unsigned char iv[UENC_IV_LEN];
    unsigned char inbuf[UENC_BUFFER_SIZE];
    unsigned char outbuf[UENC_BUFFER_SIZE + EVP_MAX_BLOCK_LENGTH];
    int outlen = 0;

    if (derive_key_iv(password, salt, key, iv) != UENC_SUCCESS) goto cleanup;

    ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { log_openssl_error("EVP_CIPHER_CTX_new"); goto cleanup; }
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv) != 1) {
        log_openssl_error("EVP_DecryptInit_ex"); goto cleanup;
    }

    for (;;) {
        size_t n = fread(inbuf, 1, sizeof(inbuf), in);
        if (n > 0) {
            if (EVP_DecryptUpdate(ctx, outbuf, &outlen, inbuf, (int)n) != 1) {
                log_openssl_error("EVP_DecryptUpdate"); goto cleanup;
            }
            if (outlen > 0 && fwrite(outbuf, 1, (size_t)outlen, out) != (size_t)outlen) {
                fprintf(stderr, "[UEnc] write plaintext: %s\n", strerror(errno));
                goto cleanup;
            }
        }
        if (n < sizeof(inbuf)) {
            if (ferror(in)) {
                fprintf(stderr, "[UEnc] read '%s': %s\n", in_path, strerror(errno));
                goto cleanup;
            }
            break;
        }
    }

    if (EVP_DecryptFinal_ex(ctx, outbuf, &outlen) != 1) {
        fprintf(stderr, "[UEnc] decrypt final failed (wrong key or corrupted file)\n");
        goto cleanup;
    }
    if (outlen > 0 && fwrite(outbuf, 1, (size_t)outlen, out) != (size_t)outlen) {
        fprintf(stderr, "[UEnc] write final block: %s\n", strerror(errno));
        goto cleanup;
    }

    if (fflush(out) != 0) {
        fprintf(stderr, "[UEnc] flush '%s': %s\n", out_path, strerror(errno));
        goto cleanup;
    }
    rc = UENC_SUCCESS;

cleanup:
    if (ctx) EVP_CIPHER_CTX_free(ctx);
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(iv,  sizeof(iv));
    fclose(in);
    fclose(out);
    if (rc != UENC_SUCCESS) remove(out_path);
    return rc;
}

int uenc_is_encrypted_file(const char *path, int *version_out)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;

    unsigned char hdr[UENC_MAGIC_LEN + 2];
    size_t n = fread(hdr, 1, sizeof(hdr), fp);
    fclose(fp);
    if (n < sizeof(hdr)) return 0;
    if (memcmp(hdr, UENC_MAGIC, UENC_MAGIC_LEN) != 0) return 0;
    int v = hdr[UENC_MAGIC_LEN] - '0';
    if (v < UENC_VERSION_MIN || v > UENC_VERSION_MAX) return 0;
    if (hdr[UENC_MAGIC_LEN + 1] != '\n') return 0;
    if (version_out) *version_out = v;
    return 1;
}

/* ========================================================================
 *  String API (Salted__ + salt + AES-256-CBC ciphertext, base64-encoded)
 *  Compatible with: openssl enc -aes-256-cbc -salt -pbkdf2 -iter 10000
 *                              -md sha1 -base64 -A
 * ====================================================================== */

int uenc_encrypt_str(const char *plaintext, const char *password,
                     char *out_b64, size_t out_capacity)
{
    if (!plaintext || !password || !out_b64) return UENC_FAILURE;
    int plain_len = (int)strlen(plaintext);
    if (plain_len < 0 || plain_len > UENC_MAX_STR_PLAIN) {
        fprintf(stderr, "[UEnc] plaintext too long (%d)\n", plain_len);
        return UENC_FAILURE;
    }

    unsigned char salt[UENC_SALT_LEN];
    unsigned char key[UENC_KEY_LEN];
    unsigned char iv[UENC_IV_LEN];

    if (RAND_bytes(salt, UENC_SALT_LEN) != 1) {
        log_openssl_error("RAND_bytes");
        return UENC_FAILURE;
    }
    if (derive_key_iv(password, salt, key, iv) != UENC_SUCCESS) {
        OPENSSL_cleanse(key, sizeof(key)); OPENSSL_cleanse(iv, sizeof(iv));
        return UENC_FAILURE;
    }

    int raw_capacity = UENC_SALTED_STR_LEN + UENC_SALT_LEN +
                       plain_len + (int)EVP_MAX_BLOCK_LENGTH;
    unsigned char *raw = (unsigned char *)malloc((size_t)raw_capacity);
    if (!raw) {
        OPENSSL_cleanse(key, sizeof(key)); OPENSSL_cleanse(iv, sizeof(iv));
        return UENC_FAILURE;
    }

    int rc = UENC_FAILURE;
    memcpy(raw, UENC_SALTED_STR, UENC_SALTED_STR_LEN);
    memcpy(raw + UENC_SALTED_STR_LEN, salt, UENC_SALT_LEN);

    int ct_len = 0;
    if (aes_encrypt_buf((const unsigned char *)plaintext, plain_len,
                        key, iv,
                        raw + UENC_SALTED_STR_LEN + UENC_SALT_LEN,
                        &ct_len) != UENC_SUCCESS) {
        goto done;
    }
    int raw_total = UENC_SALTED_STR_LEN + UENC_SALT_LEN + ct_len;
    if (b64_encode(raw, raw_total, out_b64, out_capacity) < 0) {
        goto done;
    }
    rc = UENC_SUCCESS;
done:
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(iv,  sizeof(iv));
    free(raw);
    return rc;
}

int uenc_decrypt_str(const char *b64_input, const char *password,
                     char *out_plain, size_t out_capacity)
{
    if (!b64_input || !password || !out_plain) return UENC_FAILURE;

    int b64_len = (int)strlen(b64_input);
    int max_raw = b64_len; /* base64 decodes to <= input length */
    if (max_raw < UENC_SALTED_STR_LEN + UENC_SALT_LEN + 1) {
        fprintf(stderr, "[UEnc] ciphertext too short\n");
        return UENC_FAILURE;
    }
    unsigned char *raw = (unsigned char *)malloc((size_t)max_raw);
    if (!raw) return UENC_FAILURE;

    int rc = UENC_FAILURE;
    unsigned char key[UENC_KEY_LEN];
    unsigned char iv[UENC_IV_LEN];
    unsigned char *plain = NULL;

    int raw_len = b64_decode(b64_input, raw, (size_t)max_raw);
    if (raw_len < UENC_SALTED_STR_LEN + UENC_SALT_LEN) {
        fprintf(stderr, "[UEnc] base64 decode failed or too short\n");
        goto done;
    }
    if (memcmp(raw, UENC_SALTED_STR, UENC_SALTED_STR_LEN) != 0) {
        fprintf(stderr, "[UEnc] missing Salted__ prefix\n");
        goto done;
    }
    if (derive_key_iv(password, raw + UENC_SALTED_STR_LEN,
                      key, iv) != UENC_SUCCESS) {
        goto done;
    }

    int ct_len = raw_len - (UENC_SALTED_STR_LEN + UENC_SALT_LEN);
    plain = (unsigned char *)malloc((size_t)(ct_len + EVP_MAX_BLOCK_LENGTH));
    if (!plain) goto done;

    int plain_len = 0;
    if (aes_decrypt_buf(raw + UENC_SALTED_STR_LEN + UENC_SALT_LEN,
                        ct_len, key, iv, plain, &plain_len) != UENC_SUCCESS) {
        goto done;
    }
    if ((size_t)plain_len + 1 > out_capacity) {
        fprintf(stderr, "[UEnc] plaintext output overflow (%d+1 > %zu)\n",
                plain_len, out_capacity);
        goto done;
    }
    memcpy(out_plain, plain, (size_t)plain_len);
    out_plain[plain_len] = '\0';
    rc = UENC_SUCCESS;
done:
    OPENSSL_cleanse(key, sizeof(key));
    OPENSSL_cleanse(iv,  sizeof(iv));
    free(raw);
    free(plain);
    return rc;
}

/* ========================================================================
 *  Saved-key helpers (enc.key under $KEY_LOC or .)
 * ====================================================================== */

int uenc_resolve_key_path(char *out_path, size_t out_capacity)
{
    if (!out_path) return UENC_FAILURE;
    const char *dir = getenv(UENC_KEY_PATH_ENV);
    int n;
    if (dir && dir[0]) {
        n = snprintf(out_path, out_capacity, "%s/%s", dir, UENC_KEY_NAME);
    } else {
        n = snprintf(out_path, out_capacity, "./%s", UENC_KEY_NAME);
    }
    if (n < 0 || (size_t)n >= out_capacity) return UENC_FAILURE;
    return UENC_SUCCESS;
}

int uenc_save_key(const char *key)
{
    if (!key) return UENC_FAILURE;
    int klen = (int)strlen(key);
    if (klen <= 0 || klen > UENC_MAX_KEY_LEN) {
        fprintf(stderr, "[UEnc] invalid key length %d\n", klen);
        return UENC_FAILURE;
    }
    char b64[UENC_MAX_STR_B64];
    if (uenc_encrypt_str(key, UENC_SYSTEM_KEY, b64, sizeof(b64)) != UENC_SUCCESS) {
        return UENC_FAILURE;
    }
    char path[UENC_MAX_PATH];
    if (uenc_resolve_key_path(path, sizeof(path)) != UENC_SUCCESS) {
        return UENC_FAILURE;
    }
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "[UEnc] open key file '%s': %s\n", path, strerror(errno));
        return UENC_FAILURE;
    }
    size_t blen = strlen(b64);
    int ok = (fwrite(b64, 1, blen, fp) == blen);
    fclose(fp);
    if (!ok) {
        fprintf(stderr, "[UEnc] write key file '%s' failed\n", path);
        remove(path);
        return UENC_FAILURE;
    }
    chmod(path, 0600);
    return UENC_SUCCESS;
}

int uenc_load_key(char *out_key, size_t out_capacity)
{
    if (!out_key) return UENC_FAILURE;
    char path[UENC_MAX_PATH];
    if (uenc_resolve_key_path(path, sizeof(path)) != UENC_SUCCESS) {
        return UENC_FAILURE;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[UEnc] open key file '%s': %s\n", path, strerror(errno));
        return UENC_FAILURE;
    }
    char b64[UENC_MAX_STR_B64];
    size_t n = fread(b64, 1, sizeof(b64) - 1, fp);
    int eof = feof(fp);
    fclose(fp);
    if (n == 0 || !eof) {
        fprintf(stderr, "[UEnc] key file '%s' empty or oversized\n", path);
        return UENC_FAILURE;
    }
    /* Strip trailing whitespace/newlines */
    while (n > 0 && (b64[n-1] == '\n' || b64[n-1] == '\r' ||
                     b64[n-1] == ' '  || b64[n-1] == '\t')) {
        n--;
    }
    b64[n] = '\0';
    return uenc_decrypt_str(b64, UENC_SYSTEM_KEY, out_key, out_capacity);
}
