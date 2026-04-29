#!/bin/bash
# enc_tool - automated end-to-end test harness
#
# Usage:
#   test/run_tests.sh [options] [SAMPLES_DIR]
#
# Options:
#   -d, --samples-dir DIR   시험용 음원 디렉토리 명시 지정 (가장 우선)
#       --strict            합성 샘플 fallback 비활성화 (실음원 필수)
#   -h, --help              이 도움말 표시
#
# 샘플 디렉토리 결정 우선순위 (위에서 발견되면 그걸 사용):
#   1. --samples-dir DIR  또는  -d DIR
#   2. 위치 인자 (SAMPLES_DIR)
#   3. 환경변수 $ENC_TEST_SAMPLES
#   4. 스크립트 옆의 test/samples/  (있고 비어있지 않으면)
#   5. 합성 샘플 자동 생성 (--strict 일 때는 에러로 종료)
#
# 음원 디렉토리 권장 구성 (코덱당 1개 이상; 하위 디렉토리 자유):
#   <SAMPLES_DIR>/
#     ├── *.amr | *.amrnb              (AMRNB; 헤더 또는 확장자로 감지)
#     ├── *.awb | *.amrwb              (AMRWB)
#     ├── *.evs                        (EVS)
#     ├── *.pcma | *.alaw | *.al       (PCMA)
#     └── *.pcmu | *.ulaw | *.mulaw | *.ul   (PCMU)
#
# 통계: 총/통과/실패 + 섹션별 + 코덱별 + 경과시간.

set -u

# ---------------------------------------------------------------------------
# 설정
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ENC="$ROOT_DIR/sample/enc_tool.exe"
WORK_DIR="${TMPDIR:-/tmp}/enc_tool_test_$$"

SAMPLE_SRC=""
STRICT=0

show_help() {
    sed -n '2,/^$/p' "$0" | sed 's/^# \?//'
    exit 0
}

# 옵션 + 위치 인자 파싱
POSITIONAL=()
while [ $# -gt 0 ]; do
    case "$1" in
        -d|--samples-dir)
            SAMPLE_SRC="${2:-}"
            shift 2 || { echo "Error: --samples-dir 에 값이 필요합니다" >&2; exit 2; }
            ;;
        --samples-dir=*)
            SAMPLE_SRC="${1#*=}"
            shift
            ;;
        --strict)
            STRICT=1
            shift
            ;;
        -h|--help)
            show_help
            ;;
        --)
            shift
            while [ $# -gt 0 ]; do POSITIONAL+=("$1"); shift; done
            ;;
        -*)
            echo "Error: 알 수 없는 옵션 '$1'" >&2
            echo "도움말: $0 --help" >&2
            exit 2
            ;;
        *)
            POSITIONAL+=("$1")
            shift
            ;;
    esac
done

# 우선순위: 옵션 > 위치 인자 > 환경변수 > test/samples/
if [ -z "$SAMPLE_SRC" ] && [ ${#POSITIONAL[@]} -gt 0 ]; then
    SAMPLE_SRC="${POSITIONAL[0]}"
fi
if [ -z "$SAMPLE_SRC" ] && [ -n "${ENC_TEST_SAMPLES:-}" ]; then
    SAMPLE_SRC="$ENC_TEST_SAMPLES"
fi
if [ -z "$SAMPLE_SRC" ] && [ -d "$SCRIPT_DIR/samples" ] && \
   [ -n "$(find "$SCRIPT_DIR/samples" -type f ! -name '.gitkeep' 2>/dev/null | head -n 1)" ]; then
    SAMPLE_SRC="$SCRIPT_DIR/samples"
fi

# --strict 검사: 음원 디렉토리가 반드시 있어야 함
if [ "$STRICT" -eq 1 ]; then
    if [ -z "$SAMPLE_SRC" ]; then
        echo "Error: --strict 모드: 음원 디렉토리를 지정하세요" >&2
        echo "  -d <DIR>  또는  ENC_TEST_SAMPLES=<DIR>  또는  test/samples/ 에 파일을 두세요" >&2
        exit 2
    fi
    if [ ! -d "$SAMPLE_SRC" ]; then
        echo "Error: --strict 모드: '$SAMPLE_SRC' 가 디렉토리가 아닙니다" >&2
        exit 2
    fi
    if [ -z "$(find "$SAMPLE_SRC" -type f ! -name '.gitkeep' 2>/dev/null | head -n 1)" ]; then
        echo "Error: --strict 모드: '$SAMPLE_SRC' 에 음원 파일이 없습니다" >&2
        exit 2
    fi
fi

# 색상 (TTY일 때만)
if [ -t 1 ]; then
    C_RED=$(printf '\033[31m'); C_GRN=$(printf '\033[32m')
    C_YLW=$(printf '\033[33m'); C_BLU=$(printf '\033[34m')
    C_BLD=$(printf '\033[1m');  C_RST=$(printf '\033[0m')
else
    C_RED=""; C_GRN=""; C_YLW=""; C_BLU=""; C_BLD=""; C_RST=""
fi

PASS=0
FAIL=0
SKIP=0
# bash 3 호환: associative array 대신 parallel arrays
SECTIONS=()
SEC_PASS=()
SEC_FAIL=()
CUR_SEC_IDX=-1
START_TIME=$(date +%s)

# 코덱별 샘플 파일 (단순 변수)
SAMPLE_AMRNB=""; SAMPLE_AMRWB=""; SAMPLE_EVS=""
SAMPLE_PCMA="";  SAMPLE_PCMU=""

# ---------------------------------------------------------------------------
# 헬퍼
# ---------------------------------------------------------------------------
section() {
    SECTIONS+=("$1")
    SEC_PASS+=(0)
    SEC_FAIL+=(0)
    CUR_SEC_IDX=$((${#SECTIONS[@]} - 1))
    printf "\n${C_BLU}${C_BLD}=== %s ===${C_RST}\n" "$1"
}

pass() {
    PASS=$((PASS + 1))
    SEC_PASS[$CUR_SEC_IDX]=$((${SEC_PASS[$CUR_SEC_IDX]} + 1))
    printf "  ${C_GRN}[PASS]${C_RST} %s\n" "$1"
}

fail() {
    FAIL=$((FAIL + 1))
    SEC_FAIL[$CUR_SEC_IDX]=$((${SEC_FAIL[$CUR_SEC_IDX]} + 1))
    printf "  ${C_RED}[FAIL]${C_RST} %s\n" "$1"
    [ -n "${2:-}" ] && printf "         ${C_RED}└─ %s${C_RST}\n" "$2"
}

skip() {
    SKIP=$((SKIP + 1))
    printf "  ${C_YLW}[SKIP]${C_RST} %s%s\n" "$1" "${2:+ ($2)}"
}

expect_ok() {
    local desc="$1"; shift
    local out
    if out=$("$@" 2>&1); then
        pass "$desc"
        return 0
    else
        fail "$desc" "exit=$? cmd='$*' out='$(printf '%s' "$out" | head -c 200)'"
        return 1
    fi
}

expect_fail() {
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1; then
        fail "$desc (예상은 실패였는데 성공함)"
        return 1
    else
        pass "$desc"
        return 0
    fi
}

expect_same() {
    local desc="$1" f1="$2" f2="$3"
    if cmp -s "$f1" "$f2"; then
        pass "$desc"
    else
        fail "$desc" "$f1 != $f2"
    fi
}

expect_diff() {
    local desc="$1" f1="$2" f2="$3"
    if ! cmp -s "$f1" "$f2"; then
        pass "$desc"
    else
        fail "$desc" "$f1 와 $f2 가 같음 (달라야 정상)"
    fi
}

expect_prefix_hex() {
    local desc="$1" path="$2" expected_hex="$3"
    local n=$((${#expected_hex} / 2))
    local got
    got=$(head -c $n "$path" | xxd -p | tr -d '\n')
    if [ "$got" = "$expected_hex" ]; then
        pass "$desc"
    else
        fail "$desc" "expected=$expected_hex got=$got"
    fi
}

cleanup() { rm -rf "$WORK_DIR"; }
trap cleanup EXIT

# ---------------------------------------------------------------------------
# 0. 빌드 확인
# ---------------------------------------------------------------------------
section "0_build"
if [ ! -x "$ENC" ]; then
    printf "  enc_tool.exe not found. building...\n"
    if (cd "$ROOT_DIR/sample" && make >/dev/null 2>&1); then
        pass "build succeeded"
    else
        fail "build failed"
        exit 1
    fi
else
    pass "enc_tool.exe present"
fi
"$ENC" -h >/dev/null 2>&1 && pass "-h 실행" || fail "-h 실행"

# ---------------------------------------------------------------------------
# 1. 샘플 준비
# ---------------------------------------------------------------------------
section "1_samples"
mkdir -p "$WORK_DIR/samples"
mkdir -p "$WORK_DIR/keyloc"
export KEY_LOC="$WORK_DIR/keyloc"

if [ -n "$SAMPLE_SRC" ] && [ -d "$SAMPLE_SRC" ]; then
    printf "  실제 샘플 사용: %s\n" "$SAMPLE_SRC"
    cp -R "$SAMPLE_SRC"/. "$WORK_DIR/samples/" 2>/dev/null
    rm -f "$WORK_DIR/samples/.gitkeep"
    pass "샘플 디렉토리 복사"
else
    printf "  ${C_YLW}합성 샘플 생성 (실제 음원 디렉토리를 인자로 주거나 test/samples/ 에 두면 더 정확)${C_RST}\n"
    {
        printf '#!AMR\n'
        head -c 1024 /dev/urandom
    } > "$WORK_DIR/samples/sample_amrnb.amr"
    {
        printf '#!AMR-WB\n'
        head -c 1024 /dev/urandom
    } > "$WORK_DIR/samples/sample_amrwb.awb"
    {
        printf '#!EVS_MC1.0\n'
        head -c 1024 /dev/urandom
    } > "$WORK_DIR/samples/sample_evs.evs"
    head -c 1024 /dev/urandom > "$WORK_DIR/samples/sample_pcma.pcma"
    head -c 1024 /dev/urandom > "$WORK_DIR/samples/sample_pcmu.ulaw"
    pass "5종 코덱 합성 샘플 생성"
fi

# 코덱별 첫 1개 샘플 탐색
find_first() {
    local pat
    for pat in "$@"; do
        local found
        found=$(find "$WORK_DIR/samples" -type f -iname "$pat" 2>/dev/null | head -n 1)
        if [ -n "$found" ]; then
            echo "$found"; return 0
        fi
    done
    return 1
}
SAMPLE_AMRNB=$(find_first '*.amr' '*.amrnb' || true)
SAMPLE_AMRWB=$(find_first '*.awb' '*.amrwb' || true)
SAMPLE_EVS=$(find_first '*.evs' || true)
SAMPLE_PCMA=$(find_first '*.pcma' '*.alaw' '*.al' || true)
SAMPLE_PCMU=$(find_first '*.pcmu' '*.ulaw' '*.mulaw' '*.ul' || true)

print_sample() {
    local label="$1" path="$2"
    if [ -n "$path" ]; then
        printf "    %-6s = %s\n" "$label" "$path"
    else
        printf "    %-6s = ${C_YLW}(샘플 없음 — 해당 코덱 시험 스킵)${C_RST}\n" "$label"
    fi
}
print_sample AMRNB "$SAMPLE_AMRNB"
print_sample AMRWB "$SAMPLE_AMRWB"
print_sample EVS   "$SAMPLE_EVS"
print_sample PCMA  "$SAMPLE_PCMA"
print_sample PCMU  "$SAMPLE_PCMU"

# ---------------------------------------------------------------------------
# 2. 문자열 모드
# ---------------------------------------------------------------------------
section "2_string_mode"
expect_ok "-s 키 저장" "$ENC" -s skbbiz
[ -f "$KEY_LOC/enc.key" ] && pass "enc.key 파일 생성됨" || fail "enc.key 파일 없음"
loaded=$("$ENC" -l 2>&1 | sed -n 's/^Loaded key: //p')
[ "$loaded" = "skbbiz" ] && pass "-l 출력 = skbbiz" || fail "-l 출력 = '$loaded'"

PLAIN="ipageon_ga"
CT=$("$ENC" -e skbbiz "$PLAIN" 2>/dev/null)
case "$CT" in
    U2FsdGVk*) pass "-e 출력이 U2FsdGVk 로 시작 (openssl 호환)" ;;
    *)         fail "-e 출력 = '$CT'" ;;
esac

PT=$("$ENC" -d skbbiz "$CT" 2>/dev/null)
[ "$PT" = "$PLAIN" ] && pass "-e/-d 왕복 OK" || fail "-e/-d 왕복 실패 ($PT)"

CT2=$("$ENC" -E "$PLAIN" 2>/dev/null)
PT2=$("$ENC" -D "$CT2" 2>/dev/null)
[ "$PT2" = "$PLAIN" ] && pass "-E/-D (저장키) 왕복 OK" || fail "-E/-D 왕복 실패 ($PT2)"

[ "$CT" != "$CT2" ] && pass "같은 평문이라도 매번 솔트가 달라 ct 다름" \
    || fail "ct 가 같음 (솔트 재사용 의심)"

expect_fail "-d 잘못된 키는 실패해야 함" "$ENC" -d wrong_key "$CT"

if command -v openssl >/dev/null 2>&1; then
    by_openssl=$(printf '%s' "$CT" \
        | openssl enc -d -aes-256-cbc -salt -pbkdf2 -iter 10000 -md sha1 -base64 -A -k skbbiz 2>/dev/null || true)
    [ "$by_openssl" = "$PLAIN" ] && pass "openssl CLI 로 복호화 (enc_tool→openssl)" \
        || fail "openssl 복호화 실패 ($by_openssl)"

    ct_o=$(printf '%s' "$PLAIN" \
        | openssl enc -aes-256-cbc -salt -pbkdf2 -iter 10000 -md sha1 -base64 -A -k skbbiz 2>/dev/null)
    by_us=$("$ENC" -d skbbiz "$ct_o" 2>/dev/null)
    [ "$by_us" = "$PLAIN" ] && pass "enc_tool 로 복호화 (openssl→enc_tool)" \
        || fail "enc_tool 복호화 실패 ($by_us)"
else
    skip "openssl CLI 없음 — 호환성 시험 생략"
fi

# ---------------------------------------------------------------------------
# 3. 단일 파일 모드 (코덱별 왕복)
# ---------------------------------------------------------------------------
section "3_single_file_roundtrip"
single_test() {
    local codec="$1" src="$2"
    if [ -z "$src" ]; then
        skip "$codec 코덱 시험 (샘플 없음)"
        return
    fi
    local enc_file="$WORK_DIR/${codec}.enc"
    local dec_file="$WORK_DIR/${codec}.dec"
    expect_ok    "$codec -Ef → .enc"            "$ENC" -Ef "$src" "$enc_file"
    expect_prefix_hex "$codec .enc 헤더 = '#!ENC1\\n'" "$enc_file" "2321454e43310a"
    expect_ok    "$codec -Df → 복호화 파일"      "$ENC" -Df "$enc_file" "$dec_file"
    expect_same  "$codec 원본과 byte-identical"   "$src" "$dec_file"
    expect_diff  "$codec 암호문은 원본과 다름"    "$src" "$enc_file"
}
single_test AMRNB "$SAMPLE_AMRNB"
single_test AMRWB "$SAMPLE_AMRWB"
single_test EVS   "$SAMPLE_EVS"
single_test PCMA  "$SAMPLE_PCMA"
single_test PCMU  "$SAMPLE_PCMU"

# ---------------------------------------------------------------------------
# 4. 단일 파일 --in-place
# ---------------------------------------------------------------------------
section "4_single_file_in_place"
mkdir -p "$WORK_DIR/inplace_single"
inplace_test() {
    local codec="$1" src="$2"
    [ -z "$src" ] && { skip "$codec (샘플 없음)"; return; }
    local target="$WORK_DIR/inplace_single/${codec}.bin"
    cp "$src" "$target"
    local orig_sha; orig_sha=$(sha256sum "$src" | awk '{print $1}')
    expect_ok "$codec -Ef --in-place"  "$ENC" -Ef "$target" --in-place
    expect_prefix_hex "$codec in-place 헤더 = '#!ENC1\\n'" "$target" "2321454e43310a"
    expect_ok "$codec -Df --in-place"  "$ENC" -Df "$target" --in-place
    local new_sha; new_sha=$(sha256sum "$target" | awk '{print $1}')
    [ "$orig_sha" = "$new_sha" ] && pass "$codec in-place 왕복 SHA256 동일" \
        || fail "$codec in-place SHA256 불일치"
}
inplace_test AMRNB "$SAMPLE_AMRNB"
inplace_test AMRWB "$SAMPLE_AMRWB"
inplace_test EVS   "$SAMPLE_EVS"
inplace_test PCMA  "$SAMPLE_PCMA"
inplace_test PCMU  "$SAMPLE_PCMU"

# ---------------------------------------------------------------------------
# 5. -i 정보 확인
# ---------------------------------------------------------------------------
section "5_info"
info_test() {
    local codec="$1" src="$2"
    [ -z "$src" ] && { skip "$codec -i 시험 (샘플 없음)"; return; }
    local out; out=$("$ENC" -i "$src" 2>&1)
    if echo "$out" | grep -q "$codec plaintext"; then
        pass "-i 가 $codec 코덱 인식"
    else
        fail "-i 코덱 미인식 ($codec)" "out='$out'"
    fi
}
info_test AMRNB "$SAMPLE_AMRNB"
info_test AMRWB "$SAMPLE_AMRWB"
info_test EVS   "$SAMPLE_EVS"
info_test PCMA  "$SAMPLE_PCMA"
info_test PCMU  "$SAMPLE_PCMU"

ANY_SAMPLE="$SAMPLE_AMRNB"
[ -z "$ANY_SAMPLE" ] && ANY_SAMPLE="$SAMPLE_PCMA"
[ -z "$ANY_SAMPLE" ] && ANY_SAMPLE="$SAMPLE_EVS"
if [ -n "$ANY_SAMPLE" ]; then
    info_enc="$WORK_DIR/info_test.enc"
    "$ENC" -Ef "$ANY_SAMPLE" "$info_enc" >/dev/null 2>&1
    out=$("$ENC" -i "$info_enc" 2>&1)
    echo "$out" | grep -q "enc_tool ciphertext (version=1" \
        && pass "-i 암호화 파일 인식 (version=1)" \
        || fail "-i 암호화 파일 미인식" "out='$out'"
fi

# ---------------------------------------------------------------------------
# 6. 일괄 모드 (.enc 사이드카)
# ---------------------------------------------------------------------------
section "6_batch_sidecar"
BATCH_DIR="$WORK_DIR/batch1"
mkdir -p "$BATCH_DIR/sub1" "$BATCH_DIR/sub2"
copy_sample_into() {
    local codec="$1" src="$2" target_dir="$3"
    [ -z "$src" ] && return
    local ext="${src##*.}"
    cp "$src" "$target_dir/${codec}.${ext}"
}
for c in AMRNB AMRWB EVS PCMA PCMU; do
    case "$c" in
        AMRNB) src="$SAMPLE_AMRNB" ;;
        AMRWB) src="$SAMPLE_AMRWB" ;;
        EVS)   src="$SAMPLE_EVS"   ;;
        PCMA)  src="$SAMPLE_PCMA"  ;;
        PCMU)  src="$SAMPLE_PCMU"  ;;
    esac
    [ -z "$src" ] && continue
    ext="${src##*.}"
    cp "$src" "$BATCH_DIR/${c}.${ext}"
    cp "$src" "$BATCH_DIR/sub1/${c}_a.${ext}"
    cp "$src" "$BATCH_DIR/sub2/${c}_b.${ext}"
done
echo "this is not audio" > "$BATCH_DIR/notes.txt"
( cd "$BATCH_DIR" && find . -type f -not -name 'notes.txt' \
    -exec sha256sum {} \; | sort > "$WORK_DIR/batch1_audio_orig.sha256" )

audio_count=$(find "$BATCH_DIR" -type f -not -name 'notes.txt' | wc -l | tr -d ' ')
expect_ok "-Eb 일괄 암호화" "$ENC" -Eb "$BATCH_DIR"
enc_count=$(find "$BATCH_DIR" -type f -name '*.enc' | wc -l | tr -d ' ')
[ "$enc_count" = "$audio_count" ] && pass "음원 개수($audio_count)만큼 .enc 생성" \
    || fail ".enc=$enc_count audio=$audio_count"

[ -f "$BATCH_DIR/notes.txt" ] && pass "notes.txt 는 미처리 (자동 스킵)" \
    || fail "notes.txt 가 사라짐"

( cd "$BATCH_DIR" && find . -type f -not -name 'notes.txt' -not -name '*.enc' \
    -exec sha256sum {} \; | sort > "$WORK_DIR/batch1_audio_after.sha256" )
if cmp -s "$WORK_DIR/batch1_audio_orig.sha256" "$WORK_DIR/batch1_audio_after.sha256"; then
    pass "원본 음원은 사이드카 모드에서 변경되지 않음"
else
    fail "원본 음원이 변경됨"
fi

out=$("$ENC" -Eb "$BATCH_DIR" 2>&1)
echo "$out" | grep -q "already encrypted" \
    && pass "이미 암호화된 파일 자동 스킵" \
    || fail "이미 암호화된 파일 재처리됨"

expect_ok "-Db 일괄 복호화" "$ENC" -Db "$BATCH_DIR"
( cd "$BATCH_DIR" && find . -type f -not -name 'notes.txt' -not -name '*.enc' \
    -exec sha256sum {} \; | sort > "$WORK_DIR/batch1_audio_redec.sha256" )
cmp -s "$WORK_DIR/batch1_audio_orig.sha256" "$WORK_DIR/batch1_audio_redec.sha256" \
    && pass "복호화 후 원본 SHA256 와 일치" || fail "복호화 결과가 원본과 다름"

# ---------------------------------------------------------------------------
# 7. 일괄 모드 --in-place
# ---------------------------------------------------------------------------
section "7_batch_in_place"
BATCH_DIR2="$WORK_DIR/batch_inplace"
mkdir -p "$BATCH_DIR2/sub"
for c in AMRNB AMRWB EVS PCMA PCMU; do
    case "$c" in
        AMRNB) src="$SAMPLE_AMRNB" ;;
        AMRWB) src="$SAMPLE_AMRWB" ;;
        EVS)   src="$SAMPLE_EVS"   ;;
        PCMA)  src="$SAMPLE_PCMA"  ;;
        PCMU)  src="$SAMPLE_PCMU"  ;;
    esac
    [ -z "$src" ] && continue
    ext="${src##*.}"
    cp "$src" "$BATCH_DIR2/${c}.${ext}"
    cp "$src" "$BATCH_DIR2/sub/${c}.${ext}"
done
( cd "$BATCH_DIR2" && find . -type f -exec sha256sum {} \; | sort > "$WORK_DIR/batch2_orig.sha256" )

expect_ok "-Eb --in-place 암호화" "$ENC" -Eb "$BATCH_DIR2" --in-place
sidecar_count=$(find "$BATCH_DIR2" -type f -name '*.enc' | wc -l | tr -d ' ')
[ "$sidecar_count" = "0" ] && pass ".enc 사이드카 미생성 (in-place 정상)" \
    || fail "in-place 인데 사이드카가 $sidecar_count 개 생김"

all_encrypted=1
while IFS= read -r f; do
    head=$(head -c 7 "$f" | xxd -p | tr -d '\n')
    if [ "$head" != "2321454e43310a" ]; then
        all_encrypted=0
        break
    fi
done < <(find "$BATCH_DIR2" -type f)
[ "$all_encrypted" = "1" ] && pass "모든 파일이 #!ENC1 헤더로 시작" \
    || fail "일부 파일에 #!ENC1 헤더 없음"

expect_ok "-Db --in-place 복호화" "$ENC" -Db "$BATCH_DIR2" --in-place
( cd "$BATCH_DIR2" && find . -type f -exec sha256sum {} \; | sort > "$WORK_DIR/batch2_redec.sha256" )
cmp -s "$WORK_DIR/batch2_orig.sha256" "$WORK_DIR/batch2_redec.sha256" \
    && pass "in-place 왕복 후 모든 파일 SHA256 동일" \
    || fail "in-place 왕복 후 SHA256 불일치"

# ---------------------------------------------------------------------------
# 8. --codec 필터
# ---------------------------------------------------------------------------
section "8_codec_filter"
BATCH_DIR3="$WORK_DIR/batch_codec"
mkdir -p "$BATCH_DIR3"
for c in AMRNB AMRWB EVS PCMA PCMU; do
    case "$c" in
        AMRNB) src="$SAMPLE_AMRNB" ;;
        AMRWB) src="$SAMPLE_AMRWB" ;;
        EVS)   src="$SAMPLE_EVS"   ;;
        PCMA)  src="$SAMPLE_PCMA"  ;;
        PCMU)  src="$SAMPLE_PCMU"  ;;
    esac
    [ -z "$src" ] && continue
    ext="${src##*.}"
    cp "$src" "$BATCH_DIR3/${c}.${ext}"
done
out=$("$ENC" -Eb "$BATCH_DIR3" --codec amrnb 2>&1)
amrnb_enc=$(echo "$out" | grep -c "encrypted \[AMRNB\]" || true)
filter_skip=$(echo "$out" | grep -c "skip (filter)" || true)
if [ -n "$SAMPLE_AMRNB" ]; then
    [ "$amrnb_enc" -ge 1 ] && pass "--codec amrnb: AMRNB 처리됨" \
        || fail "--codec amrnb: AMRNB 미처리"
fi
[ "$filter_skip" -ge 1 ] && pass "--codec amrnb: 다른 코덱 스킵됨" \
    || fail "--codec amrnb: 필터 미동작"

# ---------------------------------------------------------------------------
# 9. 다중 디렉토리 일괄
# ---------------------------------------------------------------------------
section "9_multi_dir"
MD1="$WORK_DIR/md_a"; MD2="$WORK_DIR/md_b"; MD3="$WORK_DIR/md_c"
mkdir -p "$MD1" "$MD2" "$MD3"
for c in AMRNB EVS PCMA; do
    case "$c" in
        AMRNB) src="$SAMPLE_AMRNB" ;;
        EVS)   src="$SAMPLE_EVS"   ;;
        PCMA)  src="$SAMPLE_PCMA"  ;;
    esac
    [ -z "$src" ] && continue
    ext="${src##*.}"
    cp "$src" "$MD1/${c}.${ext}"
    cp "$src" "$MD2/${c}.${ext}"
    cp "$src" "$MD3/${c}.${ext}"
done
total_in=$(find "$MD1" "$MD2" "$MD3" -type f | wc -l | tr -d ' ')
out=$("$ENC" -Eb "$MD1" "$MD2" "$MD3" 2>&1)
done_line=$(echo "$out" | grep "==> done.")
processed=$(echo "$done_line" | sed -n 's/.*processed=\([0-9]*\).*/\1/p')
[ "$processed" = "$total_in" ] && pass "3개 디렉토리 전부 처리 ($processed/$total_in)" \
    || fail "처리 수 불일치 (processed=$processed expected=$total_in)"

"$ENC" -Db "$MD1" "$MD2" "$MD3" >/dev/null 2>&1
restored=$(find "$MD1" "$MD2" "$MD3" -type f -not -name '*.enc' | wc -l | tr -d ' ')
[ "$restored" -ge "$total_in" ] && pass "복호화 후 원본 파일 복원됨" \
    || fail "복원된 파일 수 부족 ($restored)"

# ---------------------------------------------------------------------------
# 10. 에러 케이스
# ---------------------------------------------------------------------------
section "10_error_cases"
if [ -n "$ANY_SAMPLE" ]; then
    err_enc="$WORK_DIR/err_test.enc"
    "$ENC" -Ef "$ANY_SAMPLE" "$err_enc" >/dev/null 2>&1
    expect_fail "잘못된 키로 -df 는 실패" "$ENC" -df wrong_key "$err_enc" /dev/null
    expect_fail "평문을 -Df 하면 실패" "$ENC" -Df "$ANY_SAMPLE" "$WORK_DIR/should_fail"
fi
expect_fail "존재하지 않는 파일 -Df 는 실패" "$ENC" -Df "$WORK_DIR/no_such_file" /dev/null
expect_fail "옵션 없이 실행하면 실패" "$ENC"
expect_fail "알 수 없는 플래그는 실패" "$ENC" -X foo

# ---------------------------------------------------------------------------
# 통계 요약
# ---------------------------------------------------------------------------
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

printf "\n${C_BLD}========================================${C_RST}\n"
printf "${C_BLD}  TEST SUMMARY${C_RST}\n"
printf "${C_BLD}========================================${C_RST}\n"
printf "  소요 시간    : %ds\n" "$ELAPSED"
printf "  통과         : ${C_GRN}%d${C_RST}\n" "$PASS"
printf "  실패         : ${C_RED}%d${C_RST}\n" "$FAIL"
printf "  스킵         : ${C_YLW}%d${C_RST}\n" "$SKIP"

printf "\n  섹션별:\n"
i=0
while [ $i -lt ${#SECTIONS[@]} ]; do
    sec="${SECTIONS[$i]}"
    p="${SEC_PASS[$i]}"
    f="${SEC_FAIL[$i]}"
    if [ "$f" -gt 0 ]; then
        printf "    ${C_RED}✗${C_RST} %-30s pass=%-3d fail=%d\n" "$sec" "$p" "$f"
    else
        printf "    ${C_GRN}✓${C_RST} %-30s pass=%-3d\n" "$sec" "$p"
    fi
    i=$((i + 1))
done

printf "\n  발견된 코덱 샘플:\n"
report_codec() {
    local label="$1" path="$2"
    if [ -n "$path" ]; then
        printf "    ${C_GRN}●${C_RST} %s\n" "$label"
    else
        printf "    ${C_YLW}○${C_RST} %s ${C_YLW}(샘플 없음)${C_RST}\n" "$label"
    fi
}
report_codec AMRNB "$SAMPLE_AMRNB"
report_codec AMRWB "$SAMPLE_AMRWB"
report_codec EVS   "$SAMPLE_EVS"
report_codec PCMA  "$SAMPLE_PCMA"
report_codec PCMU  "$SAMPLE_PCMU"

printf "\n"
if [ "$FAIL" -eq 0 ]; then
    printf "${C_GRN}${C_BLD}>>> ALL TESTS PASSED <<<${C_RST}\n"
    exit 0
else
    printf "${C_RED}${C_BLD}>>> %d TESTS FAILED <<<${C_RST}\n" "$FAIL"
    exit 1
fi
