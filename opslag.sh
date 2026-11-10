#!/bin/sh
# Slår op i projekt_ordbogen i config.yaml.
# Henter først sha på main via ét API-kald, så raw hentes med fuldt commit-sha
# i stien (metodefejl_13: raw cacher på stien alene).
# Brug: sh opslag.sh [ord]   — uden ord skrives hele ordbogen ud.
# Kræver kun curl (forudinstalleret på Sailfish OS).
set -e

SHA=$(curl -sfSL --connect-timeout 10 -m 30 "https://api.github.com/repos/storefyr/Skrynja-1/commits/main" \
      | sed -n 's/.*"sha"[ ]*:[ ]*"\([0-9a-f]\{40\}\)".*/\1/p' | head -n 1)
if [ -z "$SHA" ]; then
    echo "FEJL: kunne ikke få fuldt sha på main via API — nægter at hente uden sha i stien (metodefejl_13)" >&2
    exit 1
fi
URL="https://raw.githubusercontent.com/storefyr/Skrynja-1/${SHA}/config.yaml"
CFG=$(curl -sfSL --connect-timeout 10 -m 30 "$URL")
echo "config.yaml læst fra revision ${SHA}" >&2

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
