#!/bin/sh
# Runs on the Jolla Phone 2026 itself (the only device).
# Usage: sh eksekver_test.sh        (selvtest)
#        sh eksekver_test.sh kør     (prøvetur — appen starter og bliver åben)
# Downloads the newest test RPM (branch test-rpm, plain curl), unpacks it into
# ~/skrynja-1-test-app (no root, no password), runs the app with --selvtest
# and prints only the verdict.
# Requires: curl (preinstalled), bsdtar or rpm2cpio+cpio.
set -e

# Opdater scriptets eget clone først, så vi ikke kører et forældet script mod et nyt byg.
# git skriver ved pull en ny fil og omdøber — den kørende skal beholder den gamle inode,
# så uden genstart ville hele kørslen køre den gamle udgave (åbent_punkt_15, målt på c82aa02)
if command -v git >/dev/null 2>&1 && [ -d "$HOME/Skrynja-1/.git" ]; then
    S0=$(git -C "$HOME/Skrynja-1" rev-parse HEAD 2>/dev/null || echo "")
    if git -C "$HOME/Skrynja-1" pull --ff-only >/dev/null 2>&1; then
        S1=$(git -C "$HOME/Skrynja-1" rev-parse HEAD 2>/dev/null || echo "")
        if [ -n "$S0" ] && [ "$S0" != "$S1" ]; then
            echo "Pull ændrede klonen — genstarter scriptet, så kørslen måler den nye udgave"
            exec sh "$0" "$@"
        fi
    else
        echo "ADVARSEL: git pull i ~/Skrynja-1 fejlede — kører muligvis forældet script" >&2
        S1=""
    fi
else
    echo "ADVARSEL: ~/Skrynja-1 er ikke et git-clone (eller git mangler) — kører scriptet som det ligger" >&2
    S1=""
fi
echo "script_revision: ${S1:-ukendt}"

REPO="storefyr/Skrynja-1"
# Artefakter hentes via test-rpm's eget commit-sha, ikke gren-navnet — raw cacher pr. sti,
# så to filer fra samme force-push ellers kan komme fra hver sit commit (metodefejl_13/17)
TR_SHA=""
TMP="/tmp/skrynja-1-test"
APP_DIR="$HOME/skrynja-1-test-app"
LOG="/tmp/skrynja-1-app.log"
VERDICT="/tmp/skrynja-1-selvtest.txt"

# hent <fil> <url> <max-sek> — fejler hårdt og skelner HTTP-fejl (curl exit 22) fra netværksfejl
# NB: rc opsamles uden negation — efter "if ! cmd" er $? altid 0 (metodefejl_15)
hent() {
    rc=0
    curl -sfSL --connect-timeout 10 -m "${3:-30}" -o "$1" "$2" || rc=$?
    if [ "$rc" -ne 0 ]; then
        if [ "$rc" -eq 22 ]; then
            echo "FEJL: HTTP-fejl (f.eks. 404) fra $2" >&2
        else
            echo "FEJL: netværk/time-out ved hentning af $2 (curl exit $rc)" >&2
        fi
        exit 1
    fi
}

rm -rf "$TMP"
mkdir -p "$TMP"
cd "$TMP"

# Hent test-rpm's commit-sha først — med fejldiagnose (HTTP-fejl vs netværk, som hent(); rc uden negation, metodefejl_15)
rc=0
curl -sfSL --connect-timeout 10 -m 30 -o tr_sha.json "https://api.github.com/repos/${REPO}/commits/test-rpm" || rc=$?
if [ "$rc" -ne 0 ]; then
    if [ "$rc" -eq 22 ]; then
        echo "FEJL: HTTP-fejl fra API (f.eks. 403 rate limit — 60 req/time uden token — eller 404)" >&2
    else
        echo "FEJL: netværk/time-out ved API-kald (curl exit $rc)" >&2
    fi
    exit 1
fi
TR_SHA=$(sed -n 's/.*"sha"[ ]*:[ ]*"\([0-9a-f]\{40\}\)".*/\1/p' tr_sha.json | head -n 1)
if [ -z "$TR_SHA" ]; then
    echo "FEJL: API-svar uden fuldt sha — nægter at hente uden sha i stien" >&2
    exit 1
fi
BASE="https://raw.githubusercontent.com/${REPO}/${TR_SHA}"
REV_URL="${BASE}/revision.txt"
RPM_URL="${BASE}/skrynja-1-test.aarch64.rpm"
TAR_URL="${BASE}/skrynja-1-test.tar.gz"
echo "Artefakter fra test-rpm commit ${TR_SHA}"

echo "Henter revision først, så artefakter knyttes til den ..."
hent revision.txt "$REV_URL" 30
REV=$(cat revision.txt)
echo "$REV" | grep -qE '^[0-9a-f]{40}$' || { echo "FEJL: revision.txt indeholder ikke et fuldt sha — nægter at fortsætte" >&2; exit 1; }
echo "byg_revision: $REV"

command -v sha256sum >/dev/null 2>&1 || { echo "FEJL: sha256sum mangler — nægter at køre uverificeret artefakt (metodefejl_13)" >&2; exit 1; }

echo "Henter ${RPM_URL} ..."
hent skrynja-1-test.aarch64.rpm "$RPM_URL" 120
hent skrynja-1-test.aarch64.rpm.sha256 "${RPM_URL}.sha256" 30
sha256sum -c skrynja-1-test.aarch64.rpm.sha256 || { echo "FEJL: sha256 passer ikke for RPM (revision $REV)" >&2; exit 1; }
echo "Downloaded: $(du -h skrynja-1-test.aarch64.rpm | cut -f1)"

echo "Pakker ud i ${APP_DIR} (ingen root, ingen kodeord) ..."
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR"
ARTEFAKT="skrynja-1-test.aarch64.rpm"
if command -v tar >/dev/null 2>&1; then
    rc=0
    curl -sfSL --connect-timeout 10 -m 60 -o skrynja-1-test.tar.gz "$TAR_URL" || rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "ADVARSEL: kunne ikke hente tar.gz (curl exit $rc) — falder tilbage til verificeret RPM" >&2
    else
        rc=0
        curl -sfSL --connect-timeout 10 -m 30 -o skrynja-1-test.tar.gz.sha256 "${TAR_URL}.sha256" || rc=$?
        if [ "$rc" -ne 0 ]; then
            echo "ADVARSEL: kunne ikke hente tar.gz.sha256 (curl exit $rc) — falder tilbage til verificeret RPM" >&2
        elif sha256sum -c skrynja-1-test.tar.gz.sha256; then
            if tar -xzf skrynja-1-test.tar.gz -C "$APP_DIR"; then
                ARTEFAKT="skrynja-1-test.tar.gz"
            else
                echo "ADVARSEL: tar kunne ikke pakke ud — falder tilbage til verificeret RPM" >&2
            fi
        else
            echo "FEJL: sha256 passer ikke for tar.gz (revision $REV)" >&2
            exit 1
        fi
    fi
fi
BIN="$APP_DIR/usr/bin/harbour-skrynja-1"
if [ ! -x "$BIN" ]; then
    # test på den binære fil, ikke på kataloget — og rent træ før RPM lægges ovenpå evt. halvt tar-træ
    rm -rf "$APP_DIR"
    mkdir -p "$APP_DIR"
    ARTEFAKT="skrynja-1-test.aarch64.rpm"
    if command -v bsdtar >/dev/null 2>&1; then
        bsdtar -xf "$TMP/skrynja-1-test.aarch64.rpm" -C "$APP_DIR"
    elif command -v rpm2cpio >/dev/null 2>&1 && command -v cpio >/dev/null 2>&1; then
        (cd "$APP_DIR" && rpm2cpio "$TMP/skrynja-1-test.aarch64.rpm" | cpio -idm)
    fi
fi
if [ ! -x "$BIN" ]; then
    echo "FEJL: $BIN ikke fundet efter udpakning (tar.gz + RPM prøvet)"
    exit 1
fi

if [ "${1:-}" = "kør" ]; then
    echo "Starter Skrynja-1 — prøvetur (Ctrl+C lukker appen)"
    exec "$BIN"
fi

echo "Kører selvtest (appen åbner kort) ..."
rm -f "$VERDICT"
: > "$LOG" 2>/dev/null || true   # truncér log, så rapporten kun viser denne kørsel
nohup "$BIN" --selvtest >/dev/null 2>&1 &
APP_PID=$!
# NB: wait nedenfor virker kun fordi APP_PID er et direkte barn af denne skal — behold spawn-linjen direkte
i=0
while [ $i -lt 30 ]; do
    if grep -qE '^VERDICT=(PASS|FAIL)$' "$VERDICT" 2>/dev/null; then
        break
    fi
    sleep 2
    i=$((i + 1))
done
# trappe: SIGTERM, baggrundsdræber efter 5s (SIGKILL), wait høster straks når barnet dør
kill $APP_PID 2>/dev/null || true
( sleep 5; kill -9 $APP_PID 2>/dev/null ) &   # dræberen aflives når wait vender tilbage — det begrænser PID-genbrugs-vinduet (kill -0 kan ikke skelne levende fra genbrugt PID)
KILLER=$!
wait $APP_PID 2>/dev/null || true   # wait kan returnere 143/137 ved signal — værnet er || true, ellers dør skallen under set -e
kill $KILLER 2>/dev/null || true
wait $KILLER 2>/dev/null || true
if [ ! -f "$VERDICT" ]; then
    echo "FEJL: intet verdict efter 60s — seneste app-log:"
    tail -30 "$LOG" 2>/dev/null || echo "(ingen log)"
    exit 1
fi

echo ""
echo "--- vigtige linjer (tool_call/fejl) fra $LOG ---"
if [ -f "$LOG" ]; then
    grep -v '/Silica/' "$LOG" | grep -E 'tool_call|[Ee]rror|[Ff]ejl' | tail -15 | cut -c1-400 || true
    echo "--- hele loggen: $LOG ($(wc -l < "$LOG") linjer; udskrevne linjer trunkeret til 400 tegn) ---"
fi

echo ""
echo "--- selvtest-verdict (exit-kode følger PASS/FAIL) ---"
if [ -f "$VERDICT" ]; then
    ARTEFAKT_SHA=$(sha256sum "$ARTEFAKT" 2>/dev/null | cut -d' ' -f1) || true
    if [ -z "$ARTEFAKT_SHA" ]; then
        echo "FEJL: kunne ikke beregne sha256 af $ARTEFAKT" >&2
        exit 1
    fi
    echo "byg_revision=$REV" >> "$VERDICT"
    echo "artefakt_commit=$TR_SHA" >> "$VERDICT"
    echo "artefakt_fil=$ARTEFAKT" >> "$VERDICT"
    echo "artefakt_sha256=$ARTEFAKT_SHA" >> "$VERDICT"
    awk 'length($0) > 300 { print substr($0, 1, 300) "...[trunkeret]" } length($0) <= 300 { print }' "$VERDICT"
    if grep -q '^VERDICT=PASS$' "$VERDICT"; then
        exit 0
    fi
    exit 1
else
    echo "FEJL: kendelsesfil forsvandt mellem tjek og læsning"
    exit 1
fi
