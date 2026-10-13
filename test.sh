#!/bin/sh
# Behavioural tests for umask-wrapper. Usage: test.sh /path/to/umask-wrapper
set -u

BIN=${1:?usage: test.sh /path/to/umask-wrapper}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
FAILED=0

pass() { echo "ok   - $1"; }
fail() { echo "FAIL - $1"; FAILED=1; }

expect_exit() { # description expected-code actual-code
    if [ "$3" -eq "$2" ]; then pass "$1"; else fail "$1 (expected exit $2, got $3)"; fi
}

expect_perms() { # description umask-value expected-mode
    rm -f "$WORK/f"
    APP_UMASK=$2 "$BIN" touch "$WORK/f"
    mode=$(stat -c '%a' "$WORK/f" 2>/dev/null)
    if [ "$mode" = "$3" ]; then pass "$1"; else fail "$1 (expected $3, got ${mode:-none})"; fi
}

expect_perms "APP_UMASK=0077 gives 600" 0077 600
expect_perms "APP_UMASK=0002 gives 664" 0002 664
expect_perms "APP_UMASK=022 gives 644" 022 644
expect_perms "APP_UMASK=0 gives 666" 0 666

out=$(umask 0027; unset APP_UMASK; "$BIN" sh -c umask)
if [ "$out" = "0027" ]; then pass "unset APP_UMASK keeps the inherited umask"; else fail "unset APP_UMASK keeps the inherited umask (got $out)"; fi

for bad in "" abc 8 0o22 999 01000 " 022" "022 " -1 00002; do
    rm -f "$WORK/never"
    APP_UMASK=$bad "$BIN" touch "$WORK/never" 2>/dev/null
    code=$?
    if [ "$code" -eq 125 ] && [ ! -e "$WORK/never" ]; then
        pass "invalid APP_UMASK '$bad' is rejected"
    else
        fail "invalid APP_UMASK '$bad' is rejected (exit $code)"
    fi
done

"$BIN" >/dev/null 2>&1
expect_exit "no command prints usage" 125 $?

"$BIN" /nonexistent/command 2>/dev/null
expect_exit "missing command exits 127" 127 $?

printf '#!/bin/sh\n' > "$WORK/noexec"
chmod 644 "$WORK/noexec"
"$BIN" "$WORK/noexec" 2>/dev/null
expect_exit "non-executable command exits 126" 126 $?

"$BIN" sh -c 'exit 42'
expect_exit "command exit code is passed through" 42 $?

out=$("$BIN" sh -c 'echo "$1|$2"' _ "a b" "c")
if [ "$out" = "a b|c" ]; then pass "arguments are passed through unchanged"; else fail "arguments are passed through unchanged (got $out)"; fi

out=$(FOO=bar "$BIN" sh -c 'echo $FOO')
if [ "$out" = "bar" ]; then pass "environment is passed through"; else fail "environment is passed through"; fi

expect_out() { # description expected actual
    if [ "$3" = "$2" ]; then pass "$1"; else fail "$1 (expected '$2', got '$3')"; fi
}

printf 's3cret\n' > "$WORK/secret"
out=$(FILE__DB_PASS="$WORK/secret" "$BIN" sh -c 'echo "${DB_PASS-unset}|${FILE__DB_PASS-unset}"')
expect_out "FILE__NAME sets NAME and is removed" "s3cret|unset" "$out"

printf 'a b\r\n\n' > "$WORK/crlf"
out=$(FILE__X="$WORK/crlf" "$BIN" sh -c 'printf "%s|" "$X"')
expect_out "trailing CR/LF is removed, spaces kept" "a b|" "$out"

printf 'line1\nline2\n' > "$WORK/multi"
out=$(FILE__PEM="$WORK/multi" "$BIN" sh -c 'printf "%s" "$PEM" | wc -l')
expect_out "inner newlines are kept" "1" "$(echo $out)"

: > "$WORK/empty"
out=$(FILE__E="$WORK/empty" "$BIN" sh -c 'echo "${E-unset}|"')
expect_out "an empty file sets an empty value" "|" "$out"

out=$(A=old FILE__A="$WORK/secret" FILE__B="$WORK/secret" "$BIN" sh -c 'echo "$A|$B"')
expect_out "several files, overriding an existing value" "s3cret|s3cret" "$out"

out=$(FILE__S="$WORK/secret" APP_UMASK=0077 "$BIN" sh -c 'touch "$1"; echo "$S"; stat -c %a "$1"' _ "$WORK/f2")
expect_out "works together with APP_UMASK" "s3cret 600" "$(echo $out)"

for case in "missing:FILE__M=$WORK/nope" "bad name:FILE__1A=$WORK/secret" "empty name:FILE__=$WORK/secret" "directory:FILE__D=$WORK"; do
    rm -f "$WORK/never"
    env "${case#*:}" "$BIN" touch "$WORK/never" 2>/dev/null
    code=$?
    if [ "$code" -eq 125 ] && [ ! -e "$WORK/never" ]; then pass "FILE__ error (${case%%:*}) stops the command"; else fail "FILE__ error (${case%%:*}) stops the command (exit $code)"; fi
done

head -c 65537 /dev/zero | tr '\0' a > "$WORK/big"
FILE__BIG="$WORK/big" "$BIN" true 2>/dev/null
expect_exit "a file over 64 KiB is rejected" 125 $?
head -c 65536 /dev/zero | tr '\0' a > "$WORK/max"
out=$(FILE__MAX="$WORK/max" "$BIN" sh -c 'printf "%s" "$MAX" | wc -c')
expect_out "a 64 KiB file is accepted" "65536" "$(echo $out)"

printf 'a\0b' > "$WORK/nul"
FILE__N="$WORK/nul" "$BIN" true 2>/dev/null
expect_exit "a file with a NUL byte is rejected" 125 $?

err=$(FILE__M="$WORK/nope" "$BIN" true 2>&1 >/dev/null)
case "$err" in *s3cret*) fail "errors don't print file contents" ;; *) pass "errors don't print file contents" ;; esac

exit $FAILED
