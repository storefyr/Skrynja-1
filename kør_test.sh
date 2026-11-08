#!/bin/sh
# Usage: ./kør_test.sh [run-id]   (default: latest successful run on main)
# Env:   TELEFON_IP - phone WLAN IP (Developer Mode / ssh enabled, defaultuser)
set -e

REPO="storefyr/Skrynja-1"
ARTIFACT="rpm-aarch64"
RUN_ID="${1:-}"

if [ -z "$TELEFON_IP" ]; then
    echo "Set TELEFON_IP (phone WLAN IP, Developer Mode on)." >&2
    exit 1
fi

if [ -z "$RUN_ID" ]; then
    RUN_ID=$(gh run list --repo "$REPO" --branch main --limit 20 \
        --json databaseId,conclusion \
        -q '.[] | select(.conclusion == "success") | .databaseId' | head -1)
fi
echo "Run: $RUN_ID"

TMP=$(mktemp -d)
gh run download "$RUN_ID" --repo "$REPO" --name "$ARTIFACT" --dir "$TMP"
RPM=$(ls "$TMP"/*.rpm | head -1)
echo "RPM: $RPM"

PHONE="defaultuser@${TELEFON_IP}"
scp "$RPM" "${PHONE}:/home/defaultuser/"
ssh "$PHONE" "devel-su pkcon install-local --allow-reinstall /home/defaultuser/$(basename "$RPM")"

ssh "$PHONE" "cat /tmp/skrynja-1-app.log 2>/dev/null || true" > "resultat_${RUN_ID}.log" || true
rm -rf "$TMP"
echo "Installed. Result log: resultat_${RUN_ID}.log (empty until skive_rygrad adds logging). Start Skrynja-1 from the app grid."
