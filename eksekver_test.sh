#!/bin/sh
# Runs on the Jolla Phone 2026 itself (the only device).
# Usage: sh eksekver_test.sh
# Downloads the newest test RPM (branch test-rpm, plain curl) and installs it.
# Requires: curl, devel-su (preinstalled on Sailfish OS)
set -e

REPO="storefyr/Skrynja-1"
URL="https://raw.githubusercontent.com/${REPO}/test-rpm/skrynja-1-test.aarch64.rpm"
TMP="/tmp/skrynja-1-test"

rm -rf "$TMP"
mkdir -p "$TMP"
cd "$TMP"

if ! curl -sfL -o skrynja-1-test.aarch64.rpm "$URL"; then
    echo "ERROR: could not download $URL" >&2
    echo "Has the pr_byg workflow finished and pushed branch test-rpm?" >&2
    exit 1
fi
echo "Downloaded: $(du -h skrynja-1-test.aarch64.rpm | cut -f1)"

devel-su pkcon install-local --allow-reinstall "$TMP/skrynja-1-test.aarch64.rpm"

echo "--- app log (/tmp/skrynja-1-app.log) ---"
cat /tmp/skrynja-1-app.log 2>/dev/null || echo "(no log yet)"
echo "Installed. Start Skrynja-1 from the app grid."
