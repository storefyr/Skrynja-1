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
    if ! grep -qFw -- "$f" config.yaml && ! grep -qFw -- "$(basename "$f")" config.yaml; then
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
#    Kun bash case-mønstre — ingen grep -P/-E nødvendig, så tjekket også kører på telefonen
bad_keys=$(sed -n 's/^[ ]*-[ ]*\([^:]*\):.*/\1/p; s/^[ ]*\([^:]*\):.*/\1/p' config.yaml)
re="^[a-zæøå_0-9_']+$"
bad=""
while IFS= read -r key; do
    [ -z "$key" ] && continue
    [[ "$key" =~ $re ]] || bad="$bad$key\n"
done <<EOF_HK
$bad_keys
EOF_HK
if [ -n "$bad" ]; then
    echo "FAIL: nøgler ikke på dansk ortografi:"
    printf '%b' "$bad"
    fail=1
fi

# 3b) Nøgledele med ae/oe/aa må kun forekomme hvis de står i dokumentations_standard.navngivnings_konvention.udvanding_tilladt (ægte ord) — ellers er det ascii-udvanding (metodefejl_19)
# NB: kun nøgler tjekkes her, ikke filnavne (kendt begrænsning). Hvidlisten læses som listeposter indtil næste nøgle på højst 5 mellemrum; blanke linjer og hale-whitespace fjernes (tom mønsterfil må give FAIL, ikke OK)
whitelist=$(sed -n '/^[ ]*udvanding_tilladt:/,/^[ ]\{0,5\}[^ ]/p' config.yaml | grep '^[ ]*-' | sed 's/^[ ]*-[ ]*//; s/[ ]*#.*$//; s/[ ]*$//' | grep .)
tvivl=$(sed -n 's/^[ ]*-[ ]*\([^:]*\):.*/\1/p; s/^[ ]*\([^:]*\):.*/\1/p' config.yaml \
        | tr '_' '\n' | grep -E 'ae|oe|aa' | sort -u \
        | grep -vxF -f <(printf '%s\n' "$whitelist" | grep .) || true)
if [ -n "$tvivl" ]; then
    echo "FAIL: nøgledele med ae/oe/aa ikke i udvanding_tilladt (stav med æøå medmindre ægte låneord):"
    printf '%s\n' "$tvivl"
    fail=1
fi

# 4) Pegepinde: hver "se projektlogbog <post>" skal findes som post, og postens revision skal matche sha i samme "PASS på <sha>"-sætning
for peg in $(grep -o 'se projektlogbog [a-zæøå_0-9]*' config.yaml | awk '{print $3}'); do
    if ! grep -q "^  ${peg}:" projektlogbog.yaml; then
        echo "FAIL: pegepind '$peg' findes ikke som post i projektlogbog.yaml"
        fail=1
        continue
    fi
    linje=$(grep "se projektlogbog ${peg}" config.yaml | head -1)
    if printf '%s' "$linje" | grep -q 'PASS på'; then
        sha=$(printf '%s' "$linje" | grep -o 'PASS på [0-9a-f]\{40\}' | head -1 | grep -o '[0-9a-f]\{40\}')
        if [ -z "$sha" ]; then sha=$(printf '%s' "$linje" | grep -o 'PASS på [0-9a-f]\{7,\}' | head -1 | grep -o '[0-9a-f]\{7,\}'); fi
        if [ -z "$sha" ]; then
            echo "FAIL: pegepind '$peg' står i en PASS-sætning uden sha — kan ikke kontrolleres"
            fail=1
        elif [ ${#sha} -eq 40 ]; then
            grep -A2 "^  ${peg}:" projektlogbog.yaml | grep -q "revision: ${sha}$" \
                || { echo "FAIL: pegepind '$peg' har anden revision i logbogen end fuldt sha '$sha' i samme sætning"; fail=1; }
        else
            grep -A2 "^  ${peg}:" projektlogbog.yaml | grep -q "revision: ${sha}[0-9a-f]\{33\}$" \
                || { echo "FAIL: pegepind '$peg' har anden revision i logbogen end præfikset '$sha' i samme sætning"; fail=1; }
        fi
    fi
done

# 5) Udgåede proseord: "verdict", "prompt", "rate-limit" må ikke stå fritstående
for w in $(sed -n 's/^  udgået: //p' config.yaml | tr ';' '\n' | cut -d= -f1); do
    if grep -qE "(^|[^A-Za-z0-9_ÆØÅæøå-])${w}([^A-Za-z0-9_ÆØÅæøå-]|$)" projektlogbog.yaml; then
        echo "FAIL: udgået ord '$w' står i projektlogbog.yaml"
        fail=1
    fi
done
if [ "$fail" -ne 0 ]; then
    echo "config sync check: FAILED"
    exit 1
fi
echo "config sync check: OK"
