# enc-tool

AES-256-CBC encryption tool for audio files (AMRNB / AMRWB / EVS / PCMA / PCMU)
plus ENCTOOL-compatible string operations.

* Algorithm: AES-256-CBC, key+IV via PBKDF2-HMAC-SHA1, 10000 iterations,
  8-byte salt — compatible with `openssl enc -aes-256-cbc -salt -pbkdf2
  -iter 10000 -md sha1`.
* String-mode output: base64 with the `Salted__` prefix (`U2FsdGVkX1...`),
  identical to `openssl enc -base64 -A`.
* Audio-file format: `#!ENC<v>\n` magic + `Salted__` + 8B salt + ciphertext.
  `<v>` is a single decimal digit (currently 1) so additional schemes can be
  added without breaking older files.
* The original file (including any AMR/EVS codec header `#!AMR`,
  `#!AMR-WB`, `#!EVS_MC1.0`, or raw PCMA/PCMU body) is encrypted byte-for-byte;
  decryption restores it exactly.

## Layout

```
enc-tool/
├── README.md
├── include/                   # libUEnc public header (installed)
├── lib/Linux3.10.0_ICC/       # libUEnc.a (installed; ARCH-tagged)
├── sample/
│   ├── enc_tool.c             # CLI sources
│   └── Makefile               # builds enc_tool.exe
└── src/
    ├── UEnc.c / UEnc.h        # encryption library sources
    └── Makefile               # builds libUEnc.a
```

## Build (Linux)

Requires `gcc` (or any C99 compiler) and OpenSSL development headers
(`libssl-dev` / `openssl-devel`).

```sh
cd src
make clean all install      # -> ../lib/$(ARCH)/libUEnc.a, ../include/UEnc.h

cd ../sample
make clean all              # -> ./enc_tool.exe
```

`ARCH` defaults to `Linux3.10.0_ICC` (matches the existing `lib/` subdir).
Override on the command line if you target a different arch:

```sh
make ARCH=Linux_x86_64 ...
```

`sample/Makefile` will auto-invoke `src/Makefile install` if the library
hasn't been built yet, so a one-shot build also works:

```sh
cd sample && make
```

## Usage

```
String mode (ENCTOOL-compatible):
  enc_tool.exe -s <key>                       Save key to enc.key
  enc_tool.exe -l                             Load (print) saved key
  enc_tool.exe -e <key> <plaintext>           Encrypt string by key (base64 out)
  enc_tool.exe -d <key> <ciphertext>          Decrypt base64 string by key
  enc_tool.exe -E <plaintext>                 Encrypt string with saved key
  enc_tool.exe -D <ciphertext>                Decrypt string with saved key

File mode (audio: AMRNB/AMRWB/EVS/PCMA/PCMU):
  enc_tool.exe -ef <key> <inFile> [outFile]   Encrypt single file
  enc_tool.exe -df <key> <inFile> [outFile]   Decrypt single file
  enc_tool.exe -Ef <inFile> [outFile]         Encrypt file with saved key
  enc_tool.exe -Df <inFile> [outFile]         Decrypt file with saved key

Batch mode (recursive directory walk; auto codec detection):
  enc_tool.exe -eb <key> <dir> [--codec LIST] Batch encrypt with key
  enc_tool.exe -db <key> <dir>                Batch decrypt with key
  enc_tool.exe -Eb <dir> [--codec LIST]       Batch encrypt with saved key
  enc_tool.exe -Db <dir>                      Batch decrypt with saved key

Misc:
  enc_tool.exe -i <file>                      Show codec / encryption info
  enc_tool.exe -h | --help                    Show full help
```

`--codec LIST` is a comma-separated subset of `amrnb,amrwb,evs,pcma,pcmu`.
Default for batch: encrypt all detected audio (skip everything else).

The saved-key file location resolves to `$KEY_LOC/enc.key` if the
environment variable `KEY_LOC` is set, otherwise `./enc.key`. The file
contents are themselves AES-256-CBC encrypted with the built-in system
key (`SSW_SKBBIZ`) before being written.

### Examples

String mode:

```sh
$ ./enc_tool.exe -s skbbiz
save key success!!

$ ./enc_tool.exe -l
Loaded key: skbbiz

$ ./enc_tool.exe -e skbbiz ipageon_ga
U2FsdGVkX18pGca6KC1WkGFBJLO4l4Y...

$ ./enc_tool.exe -d skbbiz "U2FsdGVkX18pGca6KC1WkGFBJLO4l4Y..."
ipageon_ga

$ ./enc_tool.exe -E ipageon_ga
U2FsdGVkX1/CzguW4lyrHthihU7O+Ce0...

$ ./enc_tool.exe -D "U2FsdGVkX1/CzguW4lyrHthihU7O+Ce0..."
ipageon_ga
```

Single audio file:

```sh
$ ./enc_tool.exe -Ef ./clip.amr             # -> ./clip.amr.enc
encrypted: ./clip.amr -> ./clip.amr.enc

$ ./enc_tool.exe -Df ./clip.amr.enc         # -> ./clip.amr  (suffix stripped)
decrypted: ./clip.amr.enc -> ./clip.amr
```

Batch directory (recursive, codec-filtered):

```sh
$ ./enc_tool.exe -Eb /vmsdata/announce --codec amrnb,evs
==> batch encrypt: /vmsdata/announce  (filter=amrnb,evs)
encrypted [AMRNB]: /vmsdata/announce/000.amr -> /vmsdata/announce/000.amr.enc
encrypted [EVS]:   /vmsdata/announce/000.evs -> /vmsdata/announce/000.evs.enc
skip (filter):     /vmsdata/announce/000.awb [AMRWB]
skip (unknown codec): /vmsdata/announce/notes.txt
==> done. total=4 processed=2 skipped=2 failed=0

$ ./enc_tool.exe -Db /vmsdata/announce       # decrypt every #!ENC<v>\n file in tree
```

### File-format inspection

```sh
$ ./enc_tool.exe -i clip.amr clip.amr.enc clip.pcma random.bin
clip.amr:     AMRNB plaintext
clip.amr.enc: enc_tool ciphertext (version=1, size=71 bytes)
clip.pcma:    PCMA plaintext
random.bin:   UNKNOWN plaintext
```

## Codec detection

* AMRNB: file starts with `#!AMR\n` (single-channel, narrow-band magic).
* AMRWB: file starts with `#!AMR-WB\n`.
* EVS:   file starts with `#!EVS_MC1.0\n`.
* PCMA:  filename ends in `.pcma`, `.alaw`, or `.al`.
* PCMU:  filename ends in `.pcmu`, `.ulaw`, `.mulaw`, or `.ul`.

In batch mode, anything not matching one of the above is skipped (encrypt
mode) or only handled if it carries the `#!ENC<v>\n` magic (decrypt mode).
For one-off `-ef` / `-Ef` invocations the file is encrypted regardless of
codec — codec detection only filters batch traversals.

## OpenSSL CLI compatibility

String-mode output is fully interoperable with the `openssl enc` CLI:

```sh
$ ./enc_tool.exe -e skbbiz "hello world" \
  | openssl enc -d -aes-256-cbc -salt -pbkdf2 -iter 10000 -md sha1 -base64 -A -k skbbiz
hello world

$ echo -n "hi" | openssl enc -aes-256-cbc -salt -pbkdf2 -iter 10000 -md sha1 -base64 -A -k skbbiz \
  | xargs ./enc_tool.exe -d skbbiz
hi
```

For audio files, strip the 7-byte `#!ENC1\n` prefix and the rest is a
standard `openssl enc -aes-256-cbc -salt`-format payload.
