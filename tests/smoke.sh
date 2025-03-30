#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN=${SMALL_SHELL_BIN:-"$ROOT/small-shell"}
TMP=$(mktemp -d "${TMPDIR:-/tmp}/small-shell.XXXXXX")
TMP_PHYSICAL=$(CDPATH= cd -- "$TMP" && pwd -P)

trap 'rm -rf "$TMP"' EXIT

make -C "$ROOT" >/dev/null

fail() {
    echo "not ok - $1" >&2
    if [ -f "$TMP/$1.out" ]; then
        echo "stdout:" >&2
        sed 's/^/  /' "$TMP/$1.out" >&2
    fi
    if [ -f "$TMP/$1.err" ]; then
        echo "stderr:" >&2
        sed 's/^/  /' "$TMP/$1.err" >&2
    fi
    exit 1
}

run_case() {
    name=$1
    input=$2
    expected_stdout=$3
    expected_status=$4

    set +e
    printf "%s" "$input" | "$BIN" >"$TMP/$name.out" 2>"$TMP/$name.err"
    status=$?
    set -e

    printf "%s" "$expected_stdout" >"$TMP/$name.expected"
    cmp -s "$TMP/$name.expected" "$TMP/$name.out" || fail "$name"
    [ "$status" -eq "$expected_status" ] || fail "$name"
}

run_case builtin_cd_pwd \
"cd $TMP
pwd
" \
"$TMP_PHYSICAL
" \
0

run_case export_env_unset \
"export SMALLSH_SMOKE=ok
env | grep '^SMALLSH_SMOKE=ok$'
unset SMALLSH_SMOKE
env | grep '^SMALLSH_SMOKE=ok$'
echo \$?
" \
"SMALLSH_SMOKE=ok
1
" \
0

run_case quote_expansion \
"export WHO=world
echo \"hello \$WHO\"
echo '\$WHO'
" \
"hello world
\$WHO
" \
0

run_case last_status \
"missing-small-shell-command
echo \$?
" \
"127
" \
0

run_case pipeline \
"echo hello | tr a-z A-Z
" \
"HELLO
" \
0

run_case redirection \
"echo first > $TMP/redir.txt
echo second >> $TMP/redir.txt
cat < $TMP/redir.txt
" \
"first
second
" \
0

run_case parent_redirection_restore \
"echo file-data > $TMP/parent-in.txt
export SMALLSH_PARENT=kept < $TMP/parent-in.txt > $TMP/parent-out.txt
echo after
env | grep '^SMALLSH_PARENT=kept$'
cat $TMP/parent-out.txt
" \
"after
SMALLSH_PARENT=kept
" \
0

run_case heredoc \
"export HD=beta
cat <<EOF
alpha
\$HD
EOF
" \
"alpha
beta
" \
0

run_case quoted_heredoc \
"export HD=beta
cat <<'EOF'
alpha
\$HD
EOF
" \
"alpha
\$HD
" \
0

run_case double_quoted_heredoc \
"export HD=beta
cat <<\"EOF\"
alpha
\$HD
EOF
" \
"alpha
\$HD
" \
0

run_case partially_quoted_heredoc \
"export HD=beta
cat <<E\"OF\"
alpha
\$HD
EOF
" \
"alpha
\$HD
" \
0

run_case syntax_error_status \
"echo |
echo \$?
" \
"258
" \
0

run_case non_interactive_stdin \
"echo one
echo two
" \
"one
two
" \
0

run_case exit_builtin \
"exit 7
echo never
" \
"" \
7

run_case multi_stage_pipeline \
"printf abc | tr a-z A-Z | sed 's/B/X/' | cat
" \
"AXC" \
0

run_case redirection_order \
"echo first > $TMP/first.txt > $TMP/second.txt
cat $TMP/first.txt
cat $TMP/second.txt
echo pipe > $TMP/pipe.txt | cat
cat $TMP/pipe.txt
" \
"first
pipe
" \
0

printf '#!/bin/sh\necho should-not-run\n' >"$TMP/not-executable"
chmod 0644 "$TMP/not-executable"
run_case cannot_execute_status \
"$TMP/not-executable
echo \$?
" \
"126
" \
0

run_case signal_exit_status \
"/bin/sh -c 'kill -TERM \$\$'
echo \$?
" \
"143
" \
0

run_case conditional_connectors \
"false && echo skipped
false || echo recovered
true && echo continued
" \
"recovered
continued
" \
0

run_case malformed_conditionals \
"&& echo never
echo \$?
echo never ||
echo \$?
" \
"258
258
" \
0

run_case unsupported_operator \
"echo never & echo never
echo \$?
" \
"258
" \
0

echo "ok - smoke"
