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

run_alloc_fault()
{
    name=$1
    scope=$2
    call=$3
    input=$4
    expected=$5

    set +e
    env SMALL_SHELL_FAIL_ALLOC_SCOPE="$scope" \
        SMALL_SHELL_FAIL_ALLOC="$call" \
        "$BIN" >"$TMP/$name.out" 2>"$TMP/$name.err" <<EOF
$input
EOF
    status=$?
    set -e
    [ "$status" -eq 0 ] || fail "$name"
    printf '%s' "$expected" >"$TMP/$name.expected"
    cmp -s "$TMP/$name.expected" "$TMP/$name.out" || fail "$name"
}

run_exit_fault()
{
    name=$1
    variable=$2
    call=$3
    input=$4
    expected_status=$5

    set +e
    printf '%s' "$input" | env "$variable=$call" "$BIN" \
        >"$TMP/$name.out" 2>"$TMP/$name.err"
    status=$?
    set -e
    [ "$status" -eq "$expected_status" ] || fail "$name"
    [ ! -s "$TMP/$name.out" ] || fail "$name"
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

run_fault save_stdin SMALL_SHELL_FAIL_DUP 1 \
    "echo hidden > $TMP/save-stdin
echo \$?
echo after" \
    '1
after
'

run_fault save_stdout SMALL_SHELL_FAIL_DUP 2 \
    "echo hidden > $TMP/save-stdout
echo \$?
echo after" \
    '1
after
'

run_fault apply_stdout SMALL_SHELL_FAIL_DUP2 1 \
    "echo hidden > $TMP/apply-stdout
echo \$?
echo after" \
    '1
after
'

run_fault restore_stdin SMALL_SHELL_FAIL_DUP2 2 \
    "echo hidden > $TMP/restore-stdin
echo \$?
echo after" \
    '1
after
'

run_fault restore_stdout SMALL_SHELL_FAIL_DUP2 3 \
    "echo hidden > $TMP/restore-stdout
echo \$?
echo after" \
    '1
after
'

run_fault open_output SMALL_SHELL_FAIL_OPEN 1 \
    "echo hidden > $TMP/open-output
echo \$?
echo after" \
    '1
after
'

run_fault heredoc_flush SMALL_SHELL_FAIL_FFLUSH 1 \
    'cat <<EOF
body
EOF
echo $?
echo after' \
    '1
after
'

run_fault heredoc_seek SMALL_SHELL_FAIL_FSEEK 1 \
    'cat <<EOF
body
EOF
echo $?
echo after' \
    '1
after
'

run_alloc_fault alloc_token token 1 \
    'echo hidden
echo $?
echo after' \
    '1
after
'

run_alloc_fault alloc_parser_node parser 1 \
    'echo hidden
echo $?
echo after' \
    '1
after
'

run_alloc_fault alloc_parser_argument parser 4 \
    'echo hidden
echo $?
echo after' \
    '1
after
'

run_alloc_fault alloc_expand expand 2 \
    'echo hidden
echo $?
echo after' \
    '1
after
'

run_alloc_fault alloc_parent_builtin execute 4 \
    'export ALLOC_TEST=value
echo $?
echo after' \
    '1
after
'

run_alloc_fault alloc_external_env execute 2 \
    'true
echo $?
echo after' \
    '1
after
'

run_fault write_stdout SMALL_SHELL_FAIL_WRITE 1 \
    'echo hidden
echo $?' \
    '1
'

run_exit_fault read_input SMALL_SHELL_FAIL_READ 1 \
    'echo hidden
' \
    1

run_fault heredoc_read_failure SMALL_SHELL_FAIL_READ 11 \
    'cat <<EOF
body
EOF
echo $?
echo after' \
    '1
after
'

run_fault multiple_heredoc_read_failure SMALL_SHELL_FAIL_READ 17 \
    'cat <<ONE <<TWO
first
ONE
second
TWO
echo $?
echo after' \
    '1
after
'

set +e
printf 'cat <<EOF
body
EOF
echo never
' | env \
    SMALL_SHELL_FAIL_READ=11 SMALL_SHELL_FAIL_READ_REPEAT=1 \
    "$BIN" >"$TMP/persistent-read.out" 2>"$TMP/persistent-read.err"
status=$?
set -e
[ "$status" -eq 1 ] || fail persistent-read
[ ! -s "$TMP/persistent-read.out" ] || fail persistent-read

set +e
env SMALL_SHELL_FAIL_DUP2=2 SMALL_SHELL_FAIL_DUP2_REPEAT=1 \
    "$BIN" >"$TMP/persistent-restore.out" \
    2>"$TMP/persistent-restore.err" <<EOF
echo hidden > $TMP/persistent-restore-file
echo never
EOF
status=$?
set -e
[ "$status" -eq 1 ] || fail persistent-restore
[ ! -s "$TMP/persistent-restore.out" ] || fail persistent-restore
grep -q 'dup2' "$TMP/persistent-restore.err" || fail persistent-restore

echo "ok - pipeline faults"
