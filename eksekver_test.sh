#!/bin/sh
# Runs on the Jolla Phone 2026 itself (the only device).
# Usage: sh eksekver_test.sh
# Downloads the newest test RPM (branch test-rpm, plain curl) and installs it.
# Requires: curl, devel-su (preinstalled on Sailfish OS)
set -e

REPO="storefyr/Skrynja-1"
URL="https://raw.githubusercontent.com/${REPO}/test-rpm/skrynja-1-test.aarch64.rpm"
TMP="/tmp/skrynja-1-test"

echo "Henter ${URL} ..."
rm -rf "$TMP"
mkdir -p "$TMP"
cd "$TMP"

if ! curl -sSL --connect-timeout 10 -m 120 -o skrynja-1-test.aarch64.rpm "$URL"; then
    echo "ERROR: download failed: $URL" >&2
    exit 1
fi
echo "Downloaded: $(du -h skrynja-1-test.aarch64.rpm | cut -f1)"

devel-su pkcon install-local --allow-reinstall "$TMP/skrynja-1-test.aarch64.rpm"

if [ "${1:-}" = "selvtest" ]; then
    rm -f /tmp/skrynja-1-selvtest.txt
    harbour-skrynja-1 --selvtest >/dev/null 2>&1 &
    APP_PID=$!
    i=0
    while [ $i -lt 60 ]; do
        [ -f /tmp/skrynja-1-selvtest.txt ] && break
        sleep 2
        i=$((i + 1))
    done
    kill $APP_PID 2>/dev/null || true
    echo "--- selvtest-verdict ---"
    cat /tmp/skrynja-1-selvtest.txt 2>/dev/null || echo "FEJL: intet verdict efter 120s — se fulde log"
    exit 0
fi

LOG=/tmp/skrynja-1-app.log
echo "--- vigtige linjer (tool_call/fejl) fra $LOG ---"
if [ -f "$LOG" ]; then
    grep -E 'tool_call|[Ee]rror|[Ff]ejl' "$LOG" | tail -30 || true
    echo "--- hele loggen: $LOG ($(wc -l < "$LOG") linjer) ---"
else
    echo "(ingen log endnu)"
fi
echo "Installed. Start Skrynja-1 from the app grid."
