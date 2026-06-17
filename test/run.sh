#!/bin/bash
# Run the Solidbits test suite: start a throwaway server on a high port,
# run every test_*.py, print a pass/fail summary, clean up. Exits non-zero
# if any suite fails. Invoked by `make test`.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
bin="$root/src/solidbits"
port="${SOLIDBITS_PORT:-16379}"
data="$(mktemp -d)"
pidfile="$data/solidbits.pid"

cleanup() {
    [ -f "$pidfile" ] && kill -9 "$(cat "$pidfile")" 2>/dev/null
    rm -rf "$data"
}
trap cleanup EXIT

cd "$root"
if [ ! -x "$bin" ]; then
    echo "solidbits not built; run ./configure && make first" >&2
    exit 1
fi

# server daemonizes (forks), so this returns once the child is up
"$bin" -d "$data" -p "$pidfile" -l 127.0.0.1:$port 2>/dev/null

# wait for it to listen
for i in $(seq 1 50); do
    ss -ltn 2>/dev/null | grep -q ":$port " && break
    sleep 0.1
done
if ! ss -ltn 2>/dev/null | grep -q ":$port "; then
    echo "server failed to listen on port $port" >&2
    exit 1
fi

export SOLIDBITS_PORT="$port"
export PYTHONPATH="$root/clients/python${PYTHONPATH:+:$PYTHONPATH}"

suites_passed=0
suites_failed=0
for t in "$here"/test_*.py; do
    [ -f "$t" ] || continue
    echo "--- $(basename "$t") ---"
    if python3 "$t"; then
        suites_passed=$((suites_passed + 1))
    else
        suites_failed=$((suites_failed + 1))
    fi
done

echo
GREEN=""; RED=""; RESET=""
if [ -t 1 ]; then GREEN=$'\033[32m'; RED=$'\033[31m'; RESET=$'\033[0m'; fi
if [ "$suites_failed" -eq 0 ]; then
    echo "${GREEN}=== suites: $suites_passed passed, $suites_failed failed ===${RESET}"
else
    echo "${RED}=== suites: $suites_passed passed, $suites_failed failed ===${RESET}"
fi
[ "$suites_failed" -eq 0 ]
