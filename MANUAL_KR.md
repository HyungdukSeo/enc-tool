# enc_tool 사용 매뉴얼

음원 파일(AMRNB / AMRWB / EVS / PCMA / PCMU)을 AES-256-CBC 로 암호화/복호화하는
리눅스 CLI 툴입니다. ENCTOOL 호환 문자열 암복호 기능도 함께 제공합니다.

---

## 목차

1. [빠른 시작](#1-빠른-시작)
2. [빌드](#2-빌드)
3. [암호화 알고리즘 / 파일 포맷](#3-암호화-알고리즘--파일-포맷)
4. [명령어 레퍼런스](#4-명령어-레퍼런스) — 각 명령마다 예제 + 예상 출력
   - 4.1 [`-s` 키 저장](#41--s-key--키-저장)
   - 4.2 [`-l` 키 로드/확인](#42--l-키-로드--확인)
   - 4.3 [`-e` 키 직접지정 문자열 암호화](#43--e-key-plaintext--키-직접지정-문자열-암호화)
   - 4.4 [`-d` 키 직접지정 문자열 복호화](#44--d-key-ciphertext--키-직접지정-문자열-복호화)
   - 4.5 [`-E` 저장키 문자열 암호화](#45--e-plaintext--저장키-문자열-암호화)
   - 4.6 [`-D` 저장키 문자열 복호화](#46--d-ciphertext--저장키-문자열-복호화)
   - 4.7 [`-ef` 키 직접지정 단일 파일 암호화](#47--ef-key-infile-outfile--키-직접지정-단일-파일-암호화)
   - 4.8 [`-df` 키 직접지정 단일 파일 복호화](#48--df-key-infile-outfile--키-직접지정-단일-파일-복호화)
   - 4.9 [`-Ef` 저장키 단일 파일 암호화](#49--ef-infile-outfile--저장키-단일-파일-암호화)
   - 4.10 [`-Df` 저장키 단일 파일 복호화](#410--df-infile-outfile--저장키-단일-파일-복호화)
   - 4.11 [`-eb` 키 직접지정 폴더 일괄 암호화](#411--eb-key-dir--키-직접지정-폴더-일괄-암호화)
   - 4.12 [`-db` 키 직접지정 폴더 일괄 복호화](#412--db-key-dir--키-직접지정-폴더-일괄-복호화)
   - 4.13 [`-Eb` 저장키 폴더 일괄 암호화](#413--eb-dir--저장키-폴더-일괄-암호화)
   - 4.14 [`-Db` 저장키 폴더 일괄 복호화](#414--db-dir--저장키-폴더-일괄-복호화)
   - 4.15 [`-i` 파일 정보 확인](#415--i-file--파일-정보-확인)
   - 4.16 [`-h` / `--help`](#416--h----help)
5. [부가 옵션](#5-부가-옵션)
   - 5.1 [`--codec` 코덱 필터](#51---codec-codecs-코덱-필터)
   - 5.2 [`--in-place` 원본 파일 그 자리에서 교체](#52---in-place-원본-파일-그-자리에서-교체)
6. [코덱 자동 감지 규칙](#6-코덱-자동-감지-규칙)
7. [실전 사용 시나리오](#7-실전-사용-시나리오)
8. [openssl CLI 와 호환](#8-openssl-cli-와-호환)
9. [트러블슈팅 / FAQ](#9-트러블슈팅--faq)
10. [라이브러리(libUEnc) 직접 사용](#10-라이브러리libuenc-직접-사용)

---

## 1. 빠른 시작

```sh
# 빌드
cd enc-tool/sample && make

# 키 저장 (최초 1회)
./enc_tool.exe -s skbbiz

# 단일 파일 암호화/복호화
./enc_tool.exe -Ef /data/audio/clip.amr           # → clip.amr.enc 생성
./enc_tool.exe -Df /data/audio/clip.amr.enc       # → clip.amr 원본 복원

# 폴더 통째로 암호화 (재귀 + 코덱 자동 감지)
./enc_tool.exe -Eb /data/audio
```

---

## 2. 빌드

### 2.1 사전 준비물

- `gcc` (또는 C99 호환 컴파일러)
- OpenSSL 개발 헤더: `libssl-dev` (Debian/Ubuntu) 또는 `openssl-devel` (RHEL/CentOS)

### 2.2 디렉토리 구조

```
enc-tool/
├── README.md
├── MANUAL_KR.md                 ← 이 문서
├── include/                     ← 빌드 시 UEnc.h 설치
├── lib/Linux3.10.0_ICC/         ← 빌드 시 libUEnc.a 설치 (ARCH 별)
├── src/
│   ├── UEnc.c, UEnc.h           ← 암호화 라이브러리 소스
│   └── Makefile                 ← libUEnc.a 빌드/설치
└── sample/
    ├── enc_tool.c               ← CLI 메인
    └── Makefile                 ← enc_tool.exe 빌드
```

### 2.3 빌드 방법

**한 번에 빌드 (권장)**
```sh
cd enc-tool/sample
make                              # 라이브러리부터 자동 빌드 → ./enc_tool.exe
```

**2단계 빌드 (정석)**
```sh
cd enc-tool/src && make clean all install     # → ../lib/$(ARCH)/libUEnc.a
cd ../sample    && make clean all             # → sample/enc_tool.exe
```

**ARCH 변경 / 컴파일러 변경**
```sh
cd src && make ARCH=Linux_x86_64 CC=icc clean all install
```

---

## 3. 암호화 알고리즘 / 파일 포맷

### 3.1 알고리즘

| 항목 | 값 |
|---|---|
| 알고리즘 | AES-256-CBC |
| 키 유도 | PBKDF2-HMAC-SHA1 |
| 반복 횟수 | 10,000 |
| 솔트 | 8 bytes (난수, 매 암호화마다 새로 생성) |
| 키 / IV | 32 bytes / 16 bytes |
| 패딩 | PKCS#7 |

### 3.2 음원 파일 포맷

```
1번째 줄: "#!ENC<v>\n"        ← 이 줄만 보고 암호화 여부 판단
2번째 줄~: <salt 8B><AES-256-CBC ciphertext>

오프셋    크기    내용
0x00      5       "#!ENC"             고정 매직
0x05      1       버전 ('1' = 0x31)   향후 알고리즘 변경 시 증가
0x06      1       '\n' (0x0A)         줄 구분자 (1번째 줄 끝)
0x07      8       salt                난수 8 bytes (2번째 줄 시작)
0x0F ~    N       AES-256-CBC ciphertext (PKCS#7 padded)
```

**복호화 동작 모델:**

1. 1번째 줄 `#!ENC<v>\n` 읽어 암호화 여부 + 버전 확인
2. 2번째 줄부터 읽음 → 맨 앞 8B 가 salt, 나머지가 ciphertext
3. salt 로부터 PBKDF2 로 키+IV 도출 → ciphertext 복호화
4. 결과는 **원본 음원 파일과 바이트 단위로 100% 동일**

원본 파일(헤더 `#!AMR`/`#!AMR-WB`/`#!EVS_MC1.0` 포함, PCMA/PCMU 본문 포함)이
**통째로** 암호화됩니다. 헤더 오버헤드: 7B(`#!ENC1\n`) + 8B(salt) + 1~16B(AES 패딩)
= **총 16~31B 증가**.

> 1번째 줄만으로 암호화 여부를 판단하므로, 다음과 같이 쉘에서도 빠르게 확인 가능:
> ```sh
> head -c 7 file.amr.enc          # "#!ENC1\n" 보이면 암호화된 파일
> head -n 1 file.amr.enc           # 같은 결과 (텍스트로)
> tail -n +2 file.amr.enc | wc -c  # 1번째 줄 뺀 나머지 크기 (salt+ct)
> ```

### 3.3 문자열 포맷

```
"Salted__" + salt(8B) + ciphertext  →  base64 인코딩 (개행 없음)
→ 항상 "U2FsdGVk..." 로 시작
```

### 3.4 키 파일

- 위치: `$KEY_LOC/enc.key` (환경변수) → 미설정 시 `./enc.key`
- 내용: 사용자가 입력한 키 문자열을 시스템 키(`SSW_SKBBIZ`)로 암호화한 base64
- 권한: 0600

---

## 4. 명령어 레퍼런스

각 명령에 **실행 예시**와 **예상 출력**을 함께 표기합니다.

### 4.1 `-s <key>` — 키 저장

키를 `enc.key` 파일에 저장합니다(시스템 키로 한 번 더 암호화됨). 이후 `-l`,
`-E`, `-D`, `-Ef`, `-Df`, `-Eb`, `-Db` 명령에서 자동으로 사용됩니다.

```sh
$ ./enc_tool.exe -s skbbiz
save key success!!

$ ls -la enc.key
-rw------- 1 user user 64 May  1 12:34 enc.key
```

### 4.2 `-l` — 키 로드 / 확인

저장된 키를 복호화하여 출력합니다. 어떤 키가 저장돼 있는지 확인할 때 사용.

```sh
$ ./enc_tool.exe -l
Loaded key: skbbiz
```

키 파일이 없을 때:
```sh
$ ./enc_tool.exe -l
[UEnc] open key file './enc.key': No such file or directory
load key failed!!
```

### 4.3 `-e <key> <plaintext>` — 키 직접지정 문자열 암호화

키 파일을 사용하지 않고 명령어에 직접 키와 평문을 넣어 암호화합니다.
출력은 base64 (`U2FsdGVk...` 형태).

```sh
$ ./enc_tool.exe -e skbbiz ipageon_ga
U2FsdGVkX18pGca6KC1WkGFBJLO4l4Y4UvkHGtRr2so=
```

> 같은 평문/키 조합도 매번 솔트가 달라 출력이 매번 다릅니다.

### 4.4 `-d <key> <ciphertext>` — 키 직접지정 문자열 복호화

```sh
$ ./enc_tool.exe -d skbbiz "U2FsdGVkX18pGca6KC1WkGFBJLO4l4Y4UvkHGtRr2so="
ipageon_ga
```

키가 틀린 경우:
```sh
$ ./enc_tool.exe -d wrong_key "U2FsdGVkX18p..."
[UEnc] decrypt final failed (wrong key or corrupted data)
```

### 4.5 `-E <plaintext>` — 저장키 문자열 암호화

`enc.key` 의 키를 사용해 평문을 암호화. `-e` 와 동일한 base64 출력 형식.

```sh
$ ./enc_tool.exe -E ipageon_ga
U2FsdGVkX1/CzguW4lyrHthihU7O+Ce0bOgC5sl0iVY=
```

### 4.6 `-D <ciphertext>` — 저장키 문자열 복호화

```sh
$ ./enc_tool.exe -D "U2FsdGVkX1/CzguW4lyrHthihU7O+Ce0bOgC5sl0iVY="
ipageon_ga
```

### 4.7 `-ef <key> <inFile> [outFile]` — 키 직접지정 단일 파일 암호화

`outFile` 생략 시 기본 출력 경로는 `<inFile>.enc`.

```sh
$ ls -la /tmp/clip.amr
-rw-r--r-- 1 user user 1024 May  1 12:00 /tmp/clip.amr

$ ./enc_tool.exe -ef skbbiz /tmp/clip.amr
encrypted: /tmp/clip.amr -> /tmp/clip.amr.enc

$ ls -la /tmp/clip.amr*
-rw-r--r-- 1 user user 1024 May  1 12:00 /tmp/clip.amr
-rw-r--r-- 1 user user 1056 May  1 12:01 /tmp/clip.amr.enc   ← 24~39B 증가
```

출력 파일 명시:
```sh
$ ./enc_tool.exe -ef skbbiz /tmp/clip.amr /tmp/encrypted/clip.bin
encrypted: /tmp/clip.amr -> /tmp/encrypted/clip.bin
```

### 4.8 `-df <key> <inFile> [outFile]` — 키 직접지정 단일 파일 복호화

`outFile` 생략 시 기본 출력 경로:
- `<inFile>` 가 `.enc` 로 끝나면 그 부분 떼어낸 이름
- 아니면 `<inFile>.dec`

```sh
$ ./enc_tool.exe -df skbbiz /tmp/clip.amr.enc
decrypted: /tmp/clip.amr.enc -> /tmp/clip.amr

$ ./enc_tool.exe -df skbbiz /tmp/encrypted/clip.bin /tmp/restored.amr
decrypted: /tmp/encrypted/clip.bin -> /tmp/restored.amr
```

### 4.9 `-Ef <inFile> [outFile]` — 저장키 단일 파일 암호화

`-ef` 와 동일하지만 키를 `enc.key` 에서 자동 로드.

```sh
$ ./enc_tool.exe -Ef /data/audio/clip.amr
encrypted: /data/audio/clip.amr -> /data/audio/clip.amr.enc

$ ./enc_tool.exe -Ef /data/audio/clip.amr --in-place
encrypted in-place: /data/audio/clip.amr        ← 원본을 그 자리에서 덮어씀
```

### 4.10 `-Df <inFile> [outFile]` — 저장키 단일 파일 복호화

```sh
$ ./enc_tool.exe -Df /data/audio/clip.amr.enc
decrypted: /data/audio/clip.amr.enc -> /data/audio/clip.amr

$ ./enc_tool.exe -Df /data/audio/clip.amr --in-place
decrypted in-place: /data/audio/clip.amr        ← 암호화돼 있던 원본을 평문으로 복원
```

### 4.11 `-eb <key> <dir>...` — 키 직접지정 폴더 일괄 암호화

지정한 디렉토리(들)를 **재귀**로 돌면서 음원 파일을 모두 암호화합니다.
**여러 폴더를 한 번에 지정** 가능. 코덱 자동 감지(섹션 6) 기준으로 음원이
아닌 파일은 자동 스킵.

```sh
$ ./enc_tool.exe -eb skbbiz /data/audio
==> batch encrypt: /data/audio
encrypted [AMRNB]: /data/audio/000.amr -> /data/audio/000.amr.enc
encrypted [EVS]: /data/audio/001.evs -> /data/audio/001.evs.enc
encrypted [AMRWB]: /data/audio/sub/002.awb -> /data/audio/sub/002.awb.enc
encrypted [PCMA]: /data/audio/sub/003.pcma -> /data/audio/sub/003.pcma.enc
skip (unknown codec): /data/audio/notes.txt
==> done. total=5 processed=4 skipped=1 failed=0
```

여러 폴더 동시:
```sh
$ ./enc_tool.exe -eb skbbiz /data/audio_a /data/audio_b /data/audio_c
==> batch encrypt: /data/audio_a
encrypted [AMRNB]: /data/audio_a/clip.amr -> /data/audio_a/clip.amr.enc
==> batch encrypt: /data/audio_b
encrypted [EVS]: /data/audio_b/clip.evs -> /data/audio_b/clip.evs.enc
==> batch encrypt: /data/audio_c
encrypted [AMRWB]: /data/audio_c/clip.awb -> /data/audio_c/clip.awb.enc
==> done. total=3 processed=3 skipped=0 failed=0
```

### 4.12 `-db <key> <dir>...` — 키 직접지정 폴더 일괄 복호화

`#!ENC<v>\n` 헤더가 있는 파일만 골라서 복호화. 평문 파일은 자동 스킵.

```sh
$ ./enc_tool.exe -db skbbiz /data/audio
==> batch decrypt: /data/audio
decrypted: /data/audio/000.amr.enc -> /data/audio/000.amr
decrypted: /data/audio/001.evs.enc -> /data/audio/001.evs
skip (not encrypted): /data/audio/000.amr
skip (not encrypted): /data/audio/notes.txt
==> done. total=4 processed=2 skipped=2 failed=0
```

### 4.13 `-Eb <dir>...` — 저장키 폴더 일괄 암호화

`-eb` 와 동일하지만 키를 `enc.key` 에서 자동 로드. **이 툴의 가장 대표적인 쓰임새**.

```sh
$ ./enc_tool.exe -Eb /data/audio
==> batch encrypt: /data/audio
encrypted [AMRNB]: /data/audio/000.amr -> /data/audio/000.amr.enc
encrypted [EVS]:   /data/audio/001.evs -> /data/audio/001.evs.enc
encrypted [PCMA]:  /data/audio/002.pcma -> /data/audio/002.pcma.enc
==> done. total=3 processed=3 skipped=0 failed=0
```

코덱 필터링 + 여러 폴더:
```sh
$ ./enc_tool.exe -Eb /data/amr /data/evs --codec amrnb,evs
==> batch encrypt: /data/amr  (filter=amrnb,evs)
encrypted [AMRNB]: /data/amr/000.amr -> /data/amr/000.amr.enc
skip (filter): /data/amr/000.awb [AMRWB]
==> batch encrypt: /data/evs  (filter=amrnb,evs)
encrypted [EVS]: /data/evs/000.evs -> /data/evs/000.evs.enc
==> done. total=3 processed=2 skipped=1 failed=0
```

원본 파일을 그 자리에서 덮어쓰기 (`.enc` 사이드카 안 만듦):
```sh
$ ./enc_tool.exe -Eb /data/audio --in-place
==> batch encrypt (in-place): /data/audio
encrypted in-place [AMRNB]: /data/audio/000.amr
encrypted in-place [EVS]:   /data/audio/001.evs
encrypted in-place [PCMA]:  /data/audio/002.pcma
==> done. total=3 processed=3 skipped=0 failed=0

$ ls /data/audio                  # .enc 안 붙음. 파일명은 그대로
000.amr  001.evs  002.pcma

$ head -c 7 /data/audio/000.amr | xxd
00000000: 2321 454e 4331 0a                        #!ENC1.    ← 내용은 암호화됨
```

### 4.14 `-Db <dir>...` — 저장키 폴더 일괄 복호화

```sh
$ ./enc_tool.exe -Db /data/audio
==> batch decrypt: /data/audio
decrypted: /data/audio/000.amr.enc -> /data/audio/000.amr
decrypted: /data/audio/001.evs.enc -> /data/audio/001.evs
==> done. total=2 processed=2 skipped=0 failed=0
```

`--in-place` 로 암호화한 파일들을 다시 원본으로 되돌리기:
```sh
$ ./enc_tool.exe -Db /data/audio --in-place
==> batch decrypt (in-place): /data/audio
decrypted in-place: /data/audio/000.amr
decrypted in-place: /data/audio/001.evs
decrypted in-place: /data/audio/002.pcma
==> done. total=3 processed=3 skipped=0 failed=0
```

### 4.15 `-i <file>...` — 파일 정보 확인

코덱 종류 또는 암호화된 파일이면 버전과 크기를 출력. 여러 파일 동시 가능.

```sh
$ ./enc_tool.exe -i /data/audio/clip.amr /data/audio/clip.amr.enc /data/audio/clip.pcma /etc/hosts
/data/audio/clip.amr:     AMRNB plaintext
/data/audio/clip.amr.enc: enc_tool ciphertext (version=1, size=1056 bytes)
/data/audio/clip.pcma:    PCMA plaintext
/etc/hosts:               UNKNOWN plaintext
```

### 4.16 `-h` / `--help`

전체 도움말을 출력합니다.

```sh
$ ./enc_tool.exe -h
Usage:
  String mode (ENCTOOL-compatible):
    enc_tool.exe -s <key>                       Save key to enc.key
    enc_tool.exe -l                             Load (print) saved key
    enc_tool.exe -e <key> <plaintext>           Encrypt string by key (base64 out)
    ...
```

---

## 5. 부가 옵션

### 5.1 `--codec <CODECS>` — 코덱 필터

일괄 암호화 시 특정 코덱만 처리. 콤마로 구분 (`amrnb,amrwb,evs,pcma,pcmu`).
미지정 시 모든 지원 코덱 처리.

```sh
$ ./enc_tool.exe -Eb /data/audio --codec amrnb
==> batch encrypt: /data/audio  (filter=amrnb)
encrypted [AMRNB]: /data/audio/000.amr -> /data/audio/000.amr.enc
skip (filter): /data/audio/001.evs [EVS]
skip (filter): /data/audio/002.pcma [PCMA]
==> done. total=3 processed=1 skipped=2 failed=0
```

### 5.2 `--in-place` — 원본 파일 그 자리에서 교체

기본 동작은 `<원본>.enc` 사이드카 파일을 새로 만드는 방식. `--in-place`
지정 시 원본을 임시 파일에 쓴 뒤 **rename 으로 원자적 교체**합니다. 파일명은
그대로 유지되며 `.enc` 사이드카는 만들지 않습니다.

```sh
# 일반 모드: clip.amr 그대로 두고 clip.amr.enc 생성
$ ./enc_tool.exe -Ef /data/audio/clip.amr
encrypted: /data/audio/clip.amr -> /data/audio/clip.amr.enc

# in-place 모드: clip.amr 자체를 암호문으로 덮어씀
$ ./enc_tool.exe -Ef /data/audio/clip.amr --in-place
encrypted in-place: /data/audio/clip.amr
```

`-ef`/`-df`/`-Ef`/`-Df`/`-eb`/`-db`/`-Eb`/`-Db` 모두에서 사용 가능.

> 주의: in-place 모드에서는 단일 파일 명령에 `outFile` 인자를 같이 줄 수 없습니다.

---

## 6. 코덱 자동 감지 규칙

| 코덱 | 감지 방법 | 매직/확장자 |
|---|---|---|
| AMRNB | 파일 헤더 | `#!AMR\n` (6 bytes) |
| AMRWB | 파일 헤더 | `#!AMR-WB\n` (9 bytes) |
| EVS | 파일 헤더 | `#!EVS_MC1.0\n` (12 bytes) |
| PCMA | 확장자 (대소문자 무시) | `.pcma`, `.alaw`, `.al` |
| PCMU | 확장자 (대소문자 무시) | `.pcmu`, `.ulaw`, `.mulaw`, `.ul` |
| (UNKNOWN) | — | 위 어디에도 안 걸림 → 일괄 모드에서 스킵 |

> **PCMA/PCMU 주의**: 헤더가 없어 **확장자만으로 판단**합니다. 확장자가
> 위와 다르면 일괄 모드에서 자동으로 처리되지 않습니다. 이때는 단일 파일
> 모드(`-Ef`)로 명시적으로 지정하면 코덱 검사 없이 암호화됩니다.

---

## 7. 실전 사용 시나리오

### 7.1 안전 수칙

원본 음원은 절대 직접 건드리지 마시고, **반드시 사본을 만들어서** 시험하세요.

```sh
cp -r /data/audio /tmp/audio_test
```

### 7.2 단일 파일 왕복 시험 (가장 먼저 권장)

```sh
ENC=/path/to/enc-tool/sample/enc_tool.exe

# 키 저장
$ENC -s skbbiz                    # → "save key success!!"

# 원본 보존
cp /tmp/audio_test/clip.amr /tmp/clip.amr.orig

# 암호화
$ENC -Ef /tmp/audio_test/clip.amr /tmp/clip.amr.enc
# → encrypted: /tmp/audio_test/clip.amr -> /tmp/clip.amr.enc

# 1번째 줄 = "#!ENC1" 확인
head -n 1 /tmp/clip.amr.enc
# → #!ENC1

# 바이너리 헤더 확인 (앞 7B "#!ENC1\n", 그 뒤 8B 가 salt, 그 뒤가 ciphertext)
head -c 16 /tmp/clip.amr.enc | xxd
# 00000000: 2321 454e 4331 0a __ __ __ __ __ __ __ __  #!ENC1.<8B salt>

# 복호화
$ENC -Df /tmp/clip.amr.enc /tmp/clip.amr.dec
# → decrypted: /tmp/clip.amr.enc -> /tmp/clip.amr.dec

# 바이트 단위 동일성 확인
cmp /tmp/clip.amr.orig /tmp/clip.amr.dec && echo "OK: 바이트 동일"
```

### 7.3 폴더 통째로 일괄 암호화

```sh
# 사본 디렉토리 통째로 암호화 (.enc 사이드카 생성)
$ENC -Eb /tmp/audio_test
# → encrypted [AMRNB]: ...
# → ==> done. total=N processed=M skipped=K failed=0

# 또는 원본을 그 자리에서 덮어쓰기 (운영 배포용)
$ENC -Eb /tmp/audio_test --in-place
# → encrypted in-place [AMRNB]: ...

# 여러 폴더를 한 번에
$ENC -Eb /tmp/audio_kor /tmp/audio_eng /tmp/audio_jpn
```

### 7.4 코덱별 분리 처리

```sh
# AMRNB 파일만 암호화하고 나머지는 건드리지 않음
$ENC -Eb /tmp/audio_test --codec amrnb

# 여러 코덱 골라서
$ENC -Eb /tmp/audio_test --codec amrnb,evs,pcma
```

### 7.5 운영 적용 체크리스트

권장 절차:

1. 테스트 디렉토리 사본으로 단일 파일 왕복 OK 확인
2. 같은 사본으로 일괄 모드 OK 확인 (`==> done. ... failed=0` 보고)
3. 키 백업 (`enc.key` 파일을 안전한 곳에 보관)
4. `KEY_LOC` 환경변수로 키 파일 경로 결정 (예: `/etc/enc-tool/enc.key`)
5. 운영 디렉토리에 적용 (보통 `--in-place` 사용)
6. 무작위 샘플 몇 개를 임시 폴더에 복사해 `-Df` 로 복호화하여 원본과 비교

```sh
# 4번~6번 실제 명령
export KEY_LOC=/etc/enc-tool
sudo mkdir -p $KEY_LOC && sudo chown root:root $KEY_LOC && sudo chmod 700 $KEY_LOC
sudo $ENC -s "your_production_key"

sudo $ENC -Eb /vmsdata/announce --in-place \
  | tee /var/log/enc_tool_$(date +%Y%m%d_%H%M).log

# 샘플 검증
mkdir /tmp/verify && cp /vmsdata/announce/sample.amr /tmp/verify/
$ENC -Df /tmp/verify/sample.amr --in-place
file /tmp/verify/sample.amr           # AMR 헤더가 보여야 정상
```

### 7.6 처리 결과 요약만 보기

```sh
$ENC -Eb /tmp/audio_test 2>&1 | tail -1
# ==> done. total=N processed=M skipped=K failed=0

$ENC -Eb /tmp/audio_test 2>&1 | grep -c "^encrypted"
# 35              ← 암호화된 파일 개수

$ENC -Eb /tmp/audio_test 2>&1 | grep "failed" | head
# (실패가 있으면 그 줄 출력)
```

---

## 8. openssl CLI 와 호환

**문자열 모드** 출력은 `openssl enc` 명령과 양방향으로 호환됩니다 (`Salted__`
prefix + base64 형식).

```sh
$ ./enc_tool.exe -e skbbiz "hello world" \
  | openssl enc -d -aes-256-cbc -salt -pbkdf2 -iter 10000 -md sha1 -base64 -A -k skbbiz
hello world

$ echo -n "hi" \
  | openssl enc -aes-256-cbc -salt -pbkdf2 -iter 10000 -md sha1 -base64 -A -k skbbiz \
  | xargs ./enc_tool.exe -d skbbiz
hi
```

**파일 모드**는 `#!ENC1\n` + raw salt + ciphertext 의 자체 포맷이라 `openssl enc`
와 직접 호환되지 않습니다. 다만 1번째 줄만 떼어내면 8B salt + AES-256-CBC
ciphertext 라는 표준 구조이므로, 다음과 같이 수동 복호화가 가능합니다:

```sh
# 1번째 줄("#!ENC1\n") 잘라낸 다음 salt 8B 와 ciphertext 분리
tail -n +2 clip.amr.enc > body.bin
SALT=$(head -c 8 body.bin | xxd -p)        # hex 문자열
tail -c +9 body.bin > ciphertext.bin

# PBKDF2-SHA1 으로 key+IV 도출 (key 32B + IV 16B = 48B)
KEYIV=$(openssl kdf -keylen 48 -kdfopt digest:SHA1 \
        -kdfopt pass:skbbiz -kdfopt hexsalt:$SALT \
        -kdfopt iter:10000 PBKDF2 | tr -d ':')
KEY=${KEYIV:0:64}
IV=${KEYIV:64:32}

openssl enc -d -aes-256-cbc -K $KEY -iv $IV -in ciphertext.bin -out clip.amr.dec
cmp clip.amr.orig clip.amr.dec && echo "OK"
```

> 보통은 그냥 `enc_tool.exe -Df` 를 쓰는 게 훨씬 간편합니다. 위 쉘 변환은 디버깅
> 용도일 때만 필요합니다.

---

## 9. 트러블슈팅 / FAQ

### Q. 빌드 시 `openssl/evp.h: No such file or directory`

OpenSSL 개발 헤더 미설치.

```sh
# Debian/Ubuntu
sudo apt-get install libssl-dev

# RHEL/CentOS
sudo yum install openssl-devel
```

### Q. 빌드 시 `cannot find -lcrypto`

OpenSSL 라이브러리 위치를 못 찾는 경우. 직접 지정:

```sh
cd sample
make CFLAGS="-I/usr/local/openssl/include" \
     LDFLAGS="-L/usr/local/openssl/lib"
```

### Q. `[UEnc] decrypt final failed (wrong key or corrupted file)`

원인 둘 중 하나:
1. 비밀번호가 틀림 — `-l` 로 저장된 키 확인
2. 파일 자체가 손상됨

### Q. `[UEnc] open key file './enc.key': No such file or directory`

`-l`/`-E`/`-D`/`-Ef`/`-Df`/`-Eb`/`-Db` 명령은 저장된 키가 필요. 먼저 `-s` 실행:

```sh
./enc_tool.exe -s skbbiz
```

또는 `KEY_LOC` 환경변수가 잘못 잡혀 있을 수 있음:

```sh
echo $KEY_LOC                     # 어디를 가리키는지 확인
unset KEY_LOC                     # 기본값(./enc.key)으로 되돌림
```

### Q. 일괄 모드에서 파일이 자꾸 `skip (unknown codec)` 됨

확장자가 인식 범위 밖. PCMA/PCMU 면 확장자를 `.pcma`/`.alaw`/`.al`/`.pcmu`/
`.ulaw`/`.mulaw`/`.ul` 중 하나로 맞추거나, 단일 파일 모드(`-Ef`)로 처리.

### Q. 암호화한 파일이 원본보다 큰데 정상인가요?

정상입니다. 추가 양:
- 헤더 7B (`#!ENC1\n`) + 솔트 8B + AES 패딩 1~16B
- → **총 16~31 bytes 증가**

### Q. 같은 파일을 두 번 암호화하면 결과가 같나요?

아니요. 매번 새로운 솔트가 사용되어 암호문은 매번 다릅니다. (보안상 의도된 동작)
복호화하면 둘 다 같은 원본으로 복원.

### Q. 이미 암호화된 파일을 또 암호화하면?

- 단일 파일(`-Ef`/`-ef`): 검사 없이 한 번 더 암호화 (이중 암호화 발생)
- 일괄 모드(`-Eb`/`-eb`): `#!ENC` 헤더가 있으면 자동으로 스킵 (`skip (already encrypted)`)

### Q. `--in-place` 도중에 디스크가 가득 차면 원본이 날아가나요?

아닙니다. `--in-place` 는 다음과 같이 동작합니다:

1. 원본을 읽어 임시 파일 `<원본>.enctmp` 에 씀
2. 임시 파일 쓰기가 성공하면 `rename(<원본>.enctmp, <원본>)` 으로 원자 교체
3. 임시 파일 쓰기 실패 시 임시 파일을 지우고 원본은 그대로 유지

따라서 디스크 부족 등으로 중간 실패해도 원본 파일은 손상되지 않습니다.

### Q. 일괄 모드 도중 `Ctrl+C` 로 중단하면?

처리하던 한 파일은 임시 파일이 남을 수 있습니다 (`.enctmp` 또는 `.enc.tmp`).
다음 실행 시 자동으로 무시되며, 안전하게 수동 삭제하시면 됩니다:

```sh
find /data/audio -name '*.enctmp' -delete
```

이미 처리 완료된 파일은 영향 없습니다(원자적 rename).

---

## 10. 라이브러리(libUEnc) 직접 사용

다른 C/C++ 프로그램에서 libUEnc 를 직접 링크하려면:

```c
#include "UEnc.h"

// 음원 파일 암호화
if (uenc_encrypt_file("input.amr", "input.amr.enc",
                       "skbbiz", UENC_VERSION_DEFAULT) != UENC_SUCCESS) {
    /* 에러 처리 */
}

// 음원 파일 복호화
if (uenc_decrypt_file("input.amr.enc", "input.amr", "skbbiz") != UENC_SUCCESS) {
    /* 에러 처리 */
}

// 문자열 암호화 (base64 출력)
char b64[UENC_MAX_STR_B64];
uenc_encrypt_str("hello", "skbbiz", b64, sizeof(b64));

// 문자열 복호화
char plain[UENC_MAX_STR_PLAIN];
uenc_decrypt_str(b64, "skbbiz", plain, sizeof(plain));

// 키 파일 저장/로드
uenc_save_key("skbbiz");                          // → enc.key
char key[UENC_MAX_KEY_LEN + 1];
uenc_load_key(key, sizeof(key));                  // ← enc.key

// 암호화 여부 검사
int version;
if (uenc_is_encrypted_file("file.enc", &version)) {
    printf("ciphertext, version=%d\n", version);
}
```

### 10.1 컴파일 / 링크 (Makefile 예시)

```make
CFLAGS  = -I/path/to/enc-tool/include
LDFLAGS = -L/path/to/enc-tool/lib/Linux3.10.0_ICC
LIBS    = -lUEnc -lcrypto

myprog: myprog.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)
```

### 10.2 공개 API 전체

| 함수 | 용도 |
|---|---|
| `uenc_encrypt_file(in, out, pwd, ver)` | 파일 암호화 |
| `uenc_decrypt_file(in, out, pwd)` | 파일 복호화 |
| `uenc_is_encrypted_file(path, *ver)` | enc_tool 암호문 여부 검사 |
| `uenc_encrypt_str(plain, pwd, b64, cap)` | 문자열 암호화 (base64) |
| `uenc_decrypt_str(b64, pwd, plain, cap)` | 문자열 복호화 |
| `uenc_save_key(key)` | 키 파일 저장 |
| `uenc_load_key(out, cap)` | 키 파일 로드 |
| `uenc_resolve_key_path(out, cap)` | 키 파일 경로 계산 |

반환값은 `UENC_SUCCESS` (0) 또는 `UENC_FAILURE` (1).
