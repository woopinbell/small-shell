#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=${SMALL_SHELL_TEST_BIN:-"$ROOT/small-shell-test"}
TIMEOUT=${SMALL_SHELL_TIMEOUT_BIN:-"$ROOT/tests/timeout-runner"}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/small-shell-lifecycle.XXXXXX")
runner_pid=
child_pid=

cleanup()
{
    if [ -n "$runner_pid" ]; then
        kill -KILL "$runner_pid" 2>/dev/null || :
        wait "$runner_pid" 2>/dev/null || :
    fi
    if [ -n "$child_pid" ]; then
        kill -KILL "$child_pid" 2>/dev/null || :
    fi
    rm -rf "$TMP"
}

trap cleanup EXIT HUP INT TERM

fail()
{
    echo "not ok - $1" >&2
    if [ -f "$TMP/$1.out" ]; then
        sed 's/^/stdout: /' "$TMP/$1.out" >&2
    fi
    if [ -f "$TMP/$1.err" ]; then
        sed 's/^/stderr: /' "$TMP/$1.err" >&2
    fi
    exit 1
}

: >"$TMP/fd-pressure.in"
i=0
while [ "$i" -lt 60 ]; do
    printf 'echo parent > %s/parent\n' "$TMP" >>"$TMP/fd-pressure.in"
    printf 'printf child | cat | cat > %s/pipeline\n' "$TMP" \
        >>"$TMP/fd-pressure.in"
    printf 'cat < %s/pipeline > %s/copy\n' "$TMP" "$TMP" \
        >>"$TMP/fd-pressure.in"
    i=$((i + 1))
done
printf 'echo fd-ok\n' >>"$TMP/fd-pressure.in"

set +e
(
    ulimit -n 48
    SMALL_SHELL_CHECK_CHILDREN=1 "$TIMEOUT" 20 "$BIN" \
        <"$TMP/fd-pressure.in" >"$TMP/fd-pressure.out" \
        2>"$TMP/fd-pressure.err"
)
status=$?
set -e
[ "$status" -eq 0 ] || fail fd-pressure
printf 'fd-ok\n' >"$TMP/fd-pressure.expected"
cmp -s "$TMP/fd-pressure.expected" "$TMP/fd-pressure.out" \
    || fail fd-pressure
[ ! -s "$TMP/fd-pressure.err" ] || fail fd-pressure

printf 'sleep 30 | cat | cat\n' >"$TMP/timeout.in"
set +e
"$TIMEOUT" 1 "$BIN" <"$TMP/timeout.in" >"$TMP/timeout.out" \
    2>"$TMP/timeout.err"
status=$?
set -e
[ "$status" -eq 124 ] || fail timeout

"$TIMEOUT" 20 /bin/sh -c \
    'printf "%s\n" "$$" >"$1"; exec sleep 30' \
    timeout-child "$TMP/child.pid" &
runner_pid=$!
i=0
while [ ! -s "$TMP/child.pid" ] && [ "$i" -lt 100 ]; do
    sleep 0.01
    i=$((i + 1))
done
[ -s "$TMP/child.pid" ] || fail external-signal
child_pid=$(sed -n '1p' "$TMP/child.pid")
kill -TERM "$runner_pid"
set +e
wait "$runner_pid"
status=$?
set -e
runner_pid=
[ "$status" -eq 143 ] || fail external-signal
set +e
kill -0 "$child_pid" 2>/dev/null
alive=$?
set -e
[ "$alive" -ne 0 ] || fail external-signal
child_pid=

echo "ok - lifecycle"
