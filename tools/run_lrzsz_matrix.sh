#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI_BIN="$ROOT_DIR/build/WhYModemCli"
WORK_DIR=""
PROTOCOLS="xmodem,ymodem,zmodem"
TIMEOUT_SEC=45
KEEP_WORK=0
INCLUDE_NEGATIVE=0

usage() {
    cat <<USAGE
Usage: $0 [options]

Options:
  --cli-bin PATH       WhYModemCli path (default: build/WhYModemCli)
  --work-dir DIR       Work directory for logs and received files (default: mktemp)
  --protocols LIST     Comma-separated list: xmodem,ymodem,zmodem (default: all)
  --timeout-sec SEC    Per-process timeout in seconds (default: 45)
  --keep-work          Keep generated work directory after success
  --include-negative   Also run negative tests such as ZMODEM send without rz
  -h, --help           Show this help
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --cli-bin) CLI_BIN="$2"; shift 2 ;;
        --work-dir) WORK_DIR="$2"; shift 2 ;;
        --protocols) PROTOCOLS="$2"; shift 2 ;;
        --timeout-sec) TIMEOUT_SEC="$2"; shift 2 ;;
        --keep-work) KEEP_WORK=1; shift ;;
        --include-negative) INCLUDE_NEGATIVE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ! -x "$CLI_BIN" ]]; then
    echo "WhYModemCli not found or not executable: $CLI_BIN" >&2
    echo "Build it first: cmake --build build --target WhYModemCli" >&2
    exit 2
fi

for tool in socat sx rx sb rb sz rz timeout cmp stat dd; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "required tool not found: $tool" >&2
        exit 2
    fi
done

if [[ -z "$WORK_DIR" ]]; then
    WORK_DIR="$(mktemp -d /tmp/whymodem-lrzsz.XXXXXX)"
else
    mkdir -p "$WORK_DIR"
fi

GUI_PORT="$WORK_DIR/whymodem_gui"
CLI_PORT="$WORK_DIR/whymodem_cli"
SOCAT_PID=""
FAILURES=0
FIXTURE_DIR=""

cleanup() {
    if [[ -n "$SOCAT_PID" ]] && kill -0 "$SOCAT_PID" >/dev/null 2>&1; then
        kill "$SOCAT_PID" >/dev/null 2>&1 || true
        wait "$SOCAT_PID" >/dev/null 2>&1 || true
    fi

    if [[ "$KEEP_WORK" -eq 0 && "$FAILURES" -eq 0 ]]; then
        rm -rf "$WORK_DIR"
    else
        echo "logs: $WORK_DIR" >&2
    fi
}
trap cleanup EXIT

has_protocol() {
    case ",$PROTOCOLS," in
        *",$1,"*) return 0 ;;
        *) return 1 ;;
    esac
}

start_socat() {
    if [[ -n "$SOCAT_PID" ]] && kill -0 "$SOCAT_PID" >/dev/null 2>&1; then
        kill "$SOCAT_PID" >/dev/null 2>&1 || true
        wait "$SOCAT_PID" >/dev/null 2>&1 || true
    fi

    rm -f "$GUI_PORT" "$CLI_PORT"
    socat -d -d \
        "pty,raw,echo=0,link=$GUI_PORT" \
        "pty,raw,echo=0,link=$CLI_PORT" \
        > "$WORK_DIR/socat.log" 2>&1 &
    SOCAT_PID=$!

    for _ in $(seq 1 100); do
        if [[ -e "$GUI_PORT" && -e "$CLI_PORT" ]]; then
            return 0
        fi
        sleep 0.05
    done

    echo "socat did not create ports" >&2
    return 1
}

make_fixtures() {
    FIXTURE_DIR="$WORK_DIR/fixtures"
    mkdir -p "$FIXTURE_DIR"

    cat > "$FIXTURE_DIR/test.log" <<'LOGEOF'
WhYModem lrzsz matrix test log
line 001: xmodem ymodem zmodem
line 002: retry and padding check
line 003: end of text fixture
LOGEOF

    dd if=/dev/urandom of="$FIXTURE_DIR/firmware.bin" bs=1024 count=21 status=none
}

compare_xmodem_prefix() {
    local src="$1"
    local dst="$2"
    local size
    size="$(stat -c %s "$src")" || return 1
    cmp -n "$size" "$src" "$dst"
}

run_case() {
    local name="$1"
    shift

    echo "== $name"
    if "$@"; then
        echo "PASS $name"
    else
        echo "FAIL $name" >&2
        FAILURES=$((FAILURES + 1))
    fi
}

cli_timeout_ms() {
    echo $((TIMEOUT_SEC * 1000))
}

test_cli_send_xmodem() {
    start_socat || return 1
    local src="$FIXTURE_DIR/test.log"
    local outdir="$WORK_DIR/xmodem-send"
    mkdir -p "$outdir"

    timeout "$TIMEOUT_SEC" "$CLI_BIN" send --protocol xmodem --port "$GUI_PORT" --file "$src" \
        --timeout-ms "$(cli_timeout_ms)" --raw-log "$outdir/cli.raw" \
        > "$outdir/cli.log" 2>&1 &
    local cli_pid=$!
    sleep 0.2

    (cd "$outdir" && timeout "$TIMEOUT_SEC" rx xmodem_from_cli.bin < "$CLI_PORT" > "$CLI_PORT") \
        > "$outdir/rx.log" 2>&1
    local lrz_rc=$?
    wait "$cli_pid"
    local cli_rc=$?

    [[ "$cli_rc" -eq 0 && "$lrz_rc" -eq 0 ]] || return 1
    compare_xmodem_prefix "$src" "$outdir/xmodem_from_cli.bin"
}

test_cli_receive_xmodem() {
    start_socat || return 1
    local src="$FIXTURE_DIR/test.log"
    local outdir="$WORK_DIR/xmodem-receive"
    mkdir -p "$outdir"

    timeout "$TIMEOUT_SEC" "$CLI_BIN" receive --protocol xmodem --port "$GUI_PORT" \
        --output "$outdir/xmodem_to_cli.bin" --timeout-ms "$(cli_timeout_ms)" \
        --raw-log "$outdir/cli.raw" > "$outdir/cli.log" 2>&1 &
    local cli_pid=$!
    sleep 0.2

    (timeout "$TIMEOUT_SEC" sx "$src" < "$CLI_PORT" > "$CLI_PORT") \
        > "$outdir/sx.log" 2>&1
    local lrz_rc=$?
    wait "$cli_pid"
    local cli_rc=$?

    [[ "$cli_rc" -eq 0 && "$lrz_rc" -eq 0 ]] || return 1
    cmp "$src" "$outdir/xmodem_to_cli.bin"
}

test_cli_send_ymodem() {
    start_socat || return 1
    local src="$FIXTURE_DIR/firmware.bin"
    local outdir="$WORK_DIR/ymodem-send"
    mkdir -p "$outdir"

    timeout "$TIMEOUT_SEC" "$CLI_BIN" send --protocol ymodem --port "$GUI_PORT" --file "$src" \
        --timeout-ms "$(cli_timeout_ms)" --raw-log "$outdir/cli.raw" \
        > "$outdir/cli.log" 2>&1 &
    local cli_pid=$!
    sleep 0.2

    (cd "$outdir" && timeout "$TIMEOUT_SEC" rb < "$CLI_PORT" > "$CLI_PORT") \
        > "$outdir/rb.log" 2>&1
    local lrz_rc=$?
    wait "$cli_pid"
    local cli_rc=$?

    [[ "$cli_rc" -eq 0 && "$lrz_rc" -eq 0 ]] || return 1
    cmp "$src" "$outdir/$(basename "$src")"
}

test_cli_receive_ymodem() {
    start_socat || return 1
    local src="$FIXTURE_DIR/firmware.bin"
    local outdir="$WORK_DIR/ymodem-receive"
    mkdir -p "$outdir"

    timeout "$TIMEOUT_SEC" "$CLI_BIN" receive --protocol ymodem --port "$GUI_PORT" \
        --output "$outdir" --timeout-ms "$(cli_timeout_ms)" --raw-log "$outdir/cli.raw" \
        > "$outdir/cli.log" 2>&1 &
    local cli_pid=$!
    sleep 0.2

    (timeout "$TIMEOUT_SEC" sb "$src" < "$CLI_PORT" > "$CLI_PORT") \
        > "$outdir/sb.log" 2>&1
    local lrz_rc=$?
    wait "$cli_pid"
    local cli_rc=$?

    [[ "$cli_rc" -eq 0 && "$lrz_rc" -eq 0 ]] || return 1
    cmp "$src" "$outdir/$(basename "$src")"
}

test_cli_send_zmodem() {
    start_socat || return 1
    local src="$FIXTURE_DIR/firmware.bin"
    local outdir="$WORK_DIR/zmodem-send"
    mkdir -p "$outdir"

    timeout "$TIMEOUT_SEC" "$CLI_BIN" send --protocol zmodem --port "$GUI_PORT" --file "$src" \
        --timeout-ms "$(cli_timeout_ms)" --raw-log "$outdir/cli.raw" \
        > "$outdir/cli.log" 2>&1 &
    local cli_pid=$!
    sleep 0.2

    (cd "$outdir" && timeout "$TIMEOUT_SEC" rz < "$CLI_PORT" > "$CLI_PORT") \
        > "$outdir/rz.log" 2>&1
    local lrz_rc=$?
    wait "$cli_pid"
    local cli_rc=$?

    [[ "$cli_rc" -eq 0 && "$lrz_rc" -eq 0 ]] || return 1
    cmp "$src" "$outdir/$(basename "$src")"
}

test_cli_receive_zmodem() {
    start_socat || return 1
    local src="$FIXTURE_DIR/firmware.bin"
    local outdir="$WORK_DIR/zmodem-receive"
    mkdir -p "$outdir"

    timeout "$TIMEOUT_SEC" "$CLI_BIN" receive --protocol zmodem --port "$GUI_PORT" \
        --output "$outdir" --timeout-ms "$(cli_timeout_ms)" --raw-log "$outdir/cli.raw" \
        > "$outdir/cli.log" 2>&1 &
    local cli_pid=$!
    sleep 0.2

    (timeout "$TIMEOUT_SEC" sz "$src" < "$CLI_PORT" > "$CLI_PORT") \
        > "$outdir/sz.log" 2>&1
    local lrz_rc=$?
    wait "$cli_pid"
    local cli_rc=$?

    [[ "$cli_rc" -eq 0 && "$lrz_rc" -eq 0 ]] || return 1
    cmp "$src" "$outdir/$(basename "$src")"
}

test_cli_send_zmodem_without_rz() {
    start_socat || return 1
    local src="$FIXTURE_DIR/firmware.bin"
    local outdir="$WORK_DIR/zmodem-send-no-rz"
    mkdir -p "$outdir"

    timeout "$TIMEOUT_SEC" "$CLI_BIN" send --protocol zmodem --port "$GUI_PORT" --file "$src" \
        --timeout-ms "$(cli_timeout_ms)" --raw-log "$outdir/cli.raw" \
        > "$outdir/cli.log" 2>&1
    local cli_rc=$?

    [[ "$cli_rc" -ne 0 ]]
}

make_fixtures

echo "work dir: $WORK_DIR"
echo "cli: $CLI_BIN"
echo "protocols: $PROTOCOLS"

if has_protocol xmodem; then
    run_case "cli send XMODEM -> rx" test_cli_send_xmodem
    run_case "sx -> cli receive XMODEM" test_cli_receive_xmodem
fi

if has_protocol ymodem; then
    run_case "cli send YMODEM -> rb" test_cli_send_ymodem
    run_case "sb -> cli receive YMODEM" test_cli_receive_ymodem
fi

if has_protocol zmodem; then
    run_case "cli send ZMODEM -> rz" test_cli_send_zmodem
    run_case "sz -> cli receive ZMODEM" test_cli_receive_zmodem
fi

if [[ "$INCLUDE_NEGATIVE" -eq 1 ]]; then
    run_case "cli send ZMODEM without rz should fail" test_cli_send_zmodem_without_rz
fi

if [[ "$FAILURES" -eq 0 ]]; then
    echo "all selected lrzsz matrix tests passed"
else
    echo "$FAILURES selected lrzsz matrix test(s) failed" >&2
fi

exit "$FAILURES"
