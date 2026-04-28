#ifndef _UENC_H_
#define _UENC_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UENC_SUCCESS                0
#define UENC_FAILURE                1

/* AES-256-CBC + PBKDF2-HMAC-SHA1 parameters */
#define UENC_KEY_LEN                32
#define UENC_IV_LEN                 16
#define UENC_SALT_LEN               8
#define UENC_PBKDF2_ITER            10000
#define UENC_BUFFER_SIZE            4096

/*
 * Audio-file framing:
 *   Line 1 : "#!ENC<v>\n"   ← only marker; presence of this line means
 *                              the file is enc_tool ciphertext
 *   Line 2+: <8B salt> + <AES-256-CBC ciphertext>
 *
 * Decryption: strip line 1, read 8B salt, decrypt the rest with the
 * key+IV derived from PBKDF2(password, salt). The decryption output is
 * byte-for-byte identical to the original input file.
 */
#define UENC_MAGIC                  "#!ENC"
#define UENC_MAGIC_LEN              5
#define UENC_LINE1_LEN              (UENC_MAGIC_LEN + 1 /*ver*/ + 1 /*\n*/)
#define UENC_HEADER_LEN             (UENC_LINE1_LEN + UENC_SALT_LEN)

/* String-mode (base64) keeps the openssl-compatible "Salted__" prefix. */
#define UENC_SALTED_STR             "Salted__"
#define UENC_SALTED_STR_LEN         8

#define UENC_VERSION_DEFAULT        1
#define UENC_VERSION_MIN            1
#define UENC_VERSION_MAX            9

/* String-mode key handling */
#define UENC_SYSTEM_KEY             "SSW_SKBBIZ"
#define UENC_KEY_NAME               "enc.key"
#define UENC_KEY_PATH_ENV           "KEY_LOC"
#define UENC_MAX_KEY_LEN            256
#define UENC_MAX_STR_PLAIN          4096
#define UENC_MAX_STR_B64            8192
#define UENC_MAX_PATH               1024

/*
 * --- Audio-file API -----------------------------------------------------
 * Encrypt/decrypt entire files. The encrypted file starts with the
 * "#!ENC<v>\n" magic, followed by the openssl-compatible "Salted__"+salt
 * prefix and the AES-256-CBC ciphertext. The original file (including any
 * AMR/EVS codec header) is encrypted byte-for-byte.
 */
int uenc_encrypt_file(const char *in_path, const char *out_path,
                      const char *password, int version);
int uenc_decrypt_file(const char *in_path, const char *out_path,
                      const char *password);
int uenc_is_encrypted_file(const char *path, int *version_out);

/*
 * --- String API (base64 in/out) ----------------------------------------
 * Compatible with `openssl enc -aes-256-cbc -salt -pbkdf2 -iter 10000
 * -md sha1 -base64 -A`. Output starts with base64-of("Salted__"...) =
 * "U2FsdGVkX1...". out_capacity is the size of the caller's buffer; the
 * function NUL-terminates and returns UENC_SUCCESS or UENC_FAILURE.
 */
int uenc_encrypt_str(const char *plaintext, const char *password,
                     char *out_b64, size_t out_capacity);
int uenc_decrypt_str(const char *b64_input, const char *password,
                     char *out_plain, size_t out_capacity);

/*
 * --- Saved-key helpers --------------------------------------------------
 * uenc_save_key: encrypts `key` with UENC_SYSTEM_KEY in string-mode and
 *   writes the base64 ciphertext to <KEY_LOC>/enc.key (or ./enc.key).
 * uenc_load_key: reads & decrypts that file, writing the original key
 *   into out_key (NUL-terminated).
 */
int uenc_save_key(const char *key);
int uenc_load_key(char *out_key, size_t out_capacity);
int uenc_resolve_key_path(char *out_path, size_t out_capacity);

#ifdef __cplusplus
}
#endif

#endif /* _UENC_H_ */
