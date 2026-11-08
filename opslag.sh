#!/bin/sh
# Slår op i projekt_ordbogen i config.yaml (hentes fra main på GitHub).
# Brug: sh opslag.sh [ord]   — uden ord skrives hele ordbogen ud.
# Kræver kun curl (forudinstalleret på Sailfish OS).
set -e

URL="https://raw.githubusercontent.com/storefyr/Skrynja-1/main/config.yaml"
CFG=$(curl -sSL --connect-timeout 10 -m 30 "$URL")

if [ -n "${1:-}" ]; then
    echo "$CFG" | awk -v ord="$1" '
        /^projekt_ordbog:/ { ind = 1; next }
        ind && /^[^ ]/ { exit }
        ind && index($0, "  " ord ":") == 1 { sub(/^  [^:]*:[ ]*/, ""); print; exit }
    '
else
    echo "$CFG" | awk '
        /^projekt_ordbog:/ { ind = 1; print; next }
        ind && /^[^ ]/ { exit }
        ind { print }
    '
fi
