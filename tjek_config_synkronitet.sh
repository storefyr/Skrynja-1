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

if [ "$fail" -ne 0 ]; then
    echo "config sync check: FAILED"
    exit 1
fi
echo "config sync check: OK"
