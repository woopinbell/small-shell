#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=${SMALL_SHELL_TEST_BIN:-"$ROOT/small-shell-test"}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/small-shell-faults.XXXXXX")

trap 'rm -rf "$TMP"' EXIT HUP INT TERM

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

run_fault()
{
    name=$1
    variable=$2
    call=$3
    input=$4
    expected=$5

    set +e
    env "$variable=$call" "$BIN" >"$TMP/$name.out" 2>"$TMP/$name.err" <<EOF
$input
EOF
    status=$?
    set -e
    [ "$status" -eq 0 ] || fail "$name"
    printf '%s' "$expected" >"$TMP/$name.expected"
    cmp -s "$TMP/$name.expected" "$TMP/$name.out" || fail "$name"
}

run_fault pipe_second SMALL_SHELL_FAIL_PIPE 2 \
    'printf alpha | cat | cat
echo $?' \
    '1
'

run_fault fork_second SMALL_SHELL_FAIL_FORK 2 \
    'sleep 30 | cat | cat
echo $?' \
    '1
'

run_fault waitpid_first SMALL_SHELL_FAIL_WAITPID 1 \
    'printf alpha | cat > /dev/null
echo $?' \
    '1
'

echo "ok - pipeline faults"
