#!/bin/sh
# Runs on the Jolla Phone 2026 itself (the only device).
# Usage: sh eksekver_test.sh
# Downloads the newest test RPM (branch test-rpm, plain curl), unpacks it into
# ~/skrynja-1-test-app (no root, no password), runs the app with --selvtest
# and prints only the verdict.
# Requires: curl (preinstalled), bsdtar or rpm2cpio+cpio.
set -e

REPO="storefyr/Skrynja-1"
URL="https://raw.githubusercontent.com/${REPO}/test-rpm/skrynja-1-test.aarch64.rpm"
TMP="/tmp/skrynja-1-test"
APP_DIR="$HOME/skrynja-1-test-app"
LOG="/tmp/skrynja-1-app.log"
VERDICT="/tmp/skrynja-1-selvtest.txt"

echo "Henter ${URL} ..."
rm -rf "$TMP"
mkdir -p "$TMP"
cd "$TMP"

if ! curl -sSL --connect-timeout 10 -m 120 -o skrynja-1-test.aarch64.rpm "$URL"; then
    echo "ERROR: download failed: $URL" >&2
    exit 1
fi
echo "Downloaded: $(du -h skrynja-1-test.aarch64.rpm | cut -f1)"

REV=""
if curl -sSL --connect-timeout 10 -m 30 -o "$TMP/revision.txt" \
        "https://raw.githubusercontent.com/${REPO}/test-rpm/revision.txt" \
        && [ -s "$TMP/revision.txt" ]; then
    REV=$(cat "$TMP/revision.txt")
fi
echo "Testet revision: ${REV:-ukendt}"

echo "Pakker RPM ud i ${APP_DIR} (ingen root, ingen kodeord) ..."
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR"
if command -v bsdtar >/dev/null 2>&1; then
    bsdtar -xf "$TMP/skrynja-1-test.aarch64.rpm" -C "$APP_DIR"
elif command -v rpm2cpio >/dev/null 2>&1 && command -v cpio >/dev/null 2>&1; then
    (cd "$APP_DIR" && rpm2cpio "$TMP/skrynja-1-test.aarch64.rpm" | cpio -idm)
else
    echo "FEJL: hverken bsdtar eller rpm2cpio+cpio findes på enheden" >&2
    exit 1
fi
BIN="$APP_DIR/usr/bin/harbour-skrynja-1"
if [ ! -x "$BIN" ]; then
    echo "FEJL: $BIN ikke fundet efter udpakning"
    exit 1
fi

echo "Kører selvtest (appen åbner kort) ..."
rm -f "$VERDICT"
nohup "$BIN" --selvtest >/dev/null 2>&1 &
APP_PID=$!
i=0
while [ $i -lt 90 ]; do
    if [ -f "$VERDICT" ]; then
        break
    fi
    sleep 2
    i=$((i + 1))
done
kill $APP_PID 2>/dev/null || true

echo ""
echo "--- vigtige linjer (tool_call/fejl) fra $LOG ---"
if [ -f "$LOG" ]; then
    grep -E 'tool_call|[Ee]rror|[Ff]ejl' "$LOG" | tail -30 || true
    echo "--- hele loggen: $LOG ($(wc -l < "$LOG") linjer) ---"
else
    echo "(ingen log endnu)"
fi

echo ""
echo "--- selvtest-verdict (sidste linje; exit-kode følger PASS/FAIL) ---"
echo "revision: ${REV:-ukendt}"
if [ -f "$VERDICT" ]; then
    cat "$VERDICT"
    if grep -q 'VERDICT=PASS' "$VERDICT"; then
        exit 0
    fi
    exit 1
else
    echo "FEJL: intet verdict efter 180s"
    exit 1
fi
