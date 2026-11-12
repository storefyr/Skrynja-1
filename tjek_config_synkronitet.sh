#!/bin/bash
# Mechanical enforcement that config.yaml stays the project's single normative source.
# Usage:
#   ./tjek_config_synkronitet.sh             inventory check only
#   ./tjek_config_synkronitet.sh <base-sha>  additionally: src/qml changes must also touch config.yaml
set -u
cd "$(git rev-parse --show-toplevel)"

fail=0

# 1) Every tracked file must be mentioned in config.yaml (full path or basename).
while IFS= read -r f; do
    if ! grep -qF -- "$f" config.yaml && ! grep -qF -- "$(basename "$f")" config.yaml; then
        echo "FAIL: '$f' is not mentioned in config.yaml"
        fail=1
    fi
done < <(git -c core.quotepath=false ls-files)

# 2) With a base ref: changes under src/ or qml/ must also change config.yaml.
if [ "$#" -ge 1 ]; then
    base="$1"
    changed="$(git diff --name-only "$base"...HEAD)"
    code_changed="$(printf '%s\n' "$changed" | grep -E '^(src|qml)/' || true)"
    if [ -n "$code_changed" ] && ! printf '%s\n' "$changed" | grep -qx 'config.yaml'; then
        echo "FAIL: src/qml changed without config.yaml in the same change set:"
        printf '%s\n' "$code_changed"
        fail=1
    fi
fi

# 3) Keys may only contain Danish letters, digits, underscore, apostrophe — spelling is NOT checked
#    (æøå vs ae/oe/aa cannot be distinguished by a character class; see metodefejl_19)
bad_keys=$(sed -n 's/^[ ]*-[ ]*\([^:]*\):.*/\1/p; s/^[ ]*\([^:]*\):.*/\1/p' config.yaml | grep -vP "^[a-zæøå_0-9']+$" || true)
if [ -n "$bad_keys" ]; then
    echo "FAIL: nøgler ikke på dansk ortografi:"
    printf '%s\n' "$bad_keys"
    fail=1
fi

# 3b) Nøgledele med ae/oe/aa må kun forekomme hvis de står i sprog.udvanding_tilladt (ægte låneord) — ellers er det ascii-udvanding (metodefejl_19)
tvivl=$(sed -n 's/^[ ]*-[ ]*\([^:]*\):.*/\1/p; s/^[ ]*\([^:]*\):.*/\1/p' config.yaml \
        | tr '_' '\n' | grep -E 'ae|oe|aa' | sort -u \
        | grep -vxF -f <(sed -n '/udvanding_tilladt:/,/^[^ ]/p' config.yaml | sed -n 's/^[ ]*-[ ]*//; s/[ ]*#.*$//p') || true)
if [ -n "$tvivl" ]; then
    echo "FAIL: nøgledele med ae/oe/aa ikke i udvanding_tilladt (stav med æøå medmindre ægte låneord):"
    printf '%s\n' "$tvivl"
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    echo "config sync check: FAILED"
    exit 1
fi
echo "config sync check: OK"
