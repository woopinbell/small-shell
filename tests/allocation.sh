#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=${SMALL_SHELL_TEST_BIN:-"$ROOT/small-shell-test"}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/small-shell-allocation.XXXXXX")

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

sweep()
{
    name=$1
    scope=$2
    maximum=$3
    input=$4
    failed_output=$5
    successful_output=$6
    call=1
    failures=0
    successes=0

    printf '%s' "$failed_output" >"$TMP/$name.failed"
    printf '%s' "$successful_output" >"$TMP/$name.success"
    while [ "$call" -le "$maximum" ]; do
        set +e
        printf '%s' "$input" | env -i \
            PATH="$PATH" \
            ALLOC_SWEEP=old \
            HEREDOC_VALUE=expanded \
            SMALL_SHELL_FAIL_ALLOC_SCOPE="$scope" \
            SMALL_SHELL_FAIL_ALLOC="$call" \
            "$BIN" >"$TMP/$name.out" 2>"$TMP/$name.err"
        status=$?
        set -e
        [ "$status" -eq 0 ] || fail "$name"
        if cmp -s "$TMP/$name.failed" "$TMP/$name.out"; then
            failures=$((failures + 1))
        elif cmp -s "$TMP/$name.success" "$TMP/$name.out"; then
            successes=$((successes + 1))
        else
            fail "$name"
        fi
        call=$((call + 1))
    done
    [ "$failures" -gt 0 ] || fail "$name"
    [ "$successes" -gt 0 ] || fail "$name"
    echo "ok - allocation $name: failures=$failures successes=$successes maximum=$maximum"
}

sweep token token 40 \
    'echo marker
echo $?
echo after
' \
    '1
after
' \
    'marker
0
after
'

sweep parser parser 20 \
    'echo marker
echo $?
echo after
' \
    '1
after
' \
    'marker
0
after
'

sweep expand expand 40 \
    'echo marker
echo $?
echo after
' \
    '1
after
' \
    'marker
0
after
'

sweep heredoc_input input 6 \
    'cat <<EOF
body
EOF
echo $?
echo after
' \
    '1
after
' \
    'body
0
after
'

sweep heredoc_quoted heredoc 8 \
    'cat <<'"'"'EOF'"'"'
$HEREDOC_VALUE
EOF
echo $?
echo after
' \
    '1
after
' \
    '$HEREDOC_VALUE
0
after
'

sweep heredoc_multiple heredoc 14 \
    'cat <<ONE <<TWO
first
ONE
second
TWO
echo $?
echo after
' \
    '1
after
' \
    'second
0
after
'

sweep heredoc_unquoted heredoc 8 \
    'cat <<EOF
$HEREDOC_VALUE
EOF
echo $?
echo after
' \
    '1
after
' \
    'expanded
0
after
'

sweep execute_builtin execute 10 \
    'export ALLOC_SWEEP=new
echo $?
echo $ALLOC_SWEEP
' \
    '1
old
' \
    '0
new
'

sweep execute_external execute 30 \
    'true
echo $?
echo after
' \
    '1
after
' \
    '0
after
'

set +e
printf 'cat <<EOF\nbody\nEOF\necho never\n' | env -i \
    PATH="$PATH" \
    SMALL_SHELL_FAIL_ALLOC_SCOPE=input \
    SMALL_SHELL_FAIL_ALLOC=1 \
    SMALL_SHELL_FAIL_ALLOC_REPEAT=1 \
    "$BIN" >"$TMP/persistent-input.out" 2>"$TMP/persistent-input.err"
status=$?
set -e
[ "$status" -eq 1 ] || fail persistent-input
[ ! -s "$TMP/persistent-input.out" ] || fail persistent-input

set +e
printf 'echo hidden\n' | env -i \
    PATH="$PATH" \
    SMALL_SHELL_FAIL_ALLOC_SCOPE=token \
    SMALL_SHELL_FAIL_ALLOC=1 \
    SMALL_SHELL_FAIL_ALLOC_REPEAT=1 \
    "$BIN" >"$TMP/persistent.out" 2>"$TMP/persistent.err"
status=$?
set -e
[ "$status" -eq 1 ] || fail persistent
[ ! -s "$TMP/persistent.out" ] || fail persistent
grep -q 'allocation failure' "$TMP/persistent.err" || fail persistent

echo "ok - allocation failures"
