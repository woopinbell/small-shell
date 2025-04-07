#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=${SMALL_SHELL_BIN:-"$ROOT/small-shell"}
TIMEOUT=${SMALL_SHELL_TIMEOUT_BIN:-"$ROOT/tests/timeout-runner"}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/small-shell-performance.XXXXXX")
PAYLOAD_SIZE=524288

trap 'rm -rf "$TMP"' EXIT HUP INT TERM

fail()
{
    echo "not ok - long input performance" >&2
    if [ -f "$TMP/long.err" ]; then
        sed 's/^/stderr: /' "$TMP/long.err" >&2
    fi
    exit 1
}

{
    printf 'echo '
    dd if=/dev/zero bs=1024 count=512 2>/dev/null | tr '\000' x
    printf '\n'
} >"$TMP/long.in"

set +e
"$TIMEOUT" 5 "$BIN" <"$TMP/long.in" \
    >"$TMP/long.out" 2>"$TMP/long.err"
status=$?
set -e

[ "$status" -eq 0 ] || fail
[ ! -s "$TMP/long.err" ] || fail
output_size=$(wc -c <"$TMP/long.out" | tr -d '[:space:]')
[ "$output_size" -eq $((PAYLOAD_SIZE + 1)) ] || fail

echo "ok - long input performance"
