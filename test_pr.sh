#!/bin/sh
# Skrynja-1 PR-testscript - kør på Sailfish OS-enheden (Jolla Phone 2026, aarch64)
# Henter aarch64-RPM for en PR og installerer den.
#
# Brug:
#   sh test_pr.sh <pr-nummer>     # f.eks. sh test_pr.sh 2
#
# Kræver: curl, devel-su (forudinstalleret på Sailfish OS)
# Ingen API-kald, ingen jq, ingen python - kun direkte download fra raw.githubusercontent.com

set -e

PR="${1:-}"
REPO="storefyr/Skrynja-1"
BRANCH="pr-${PR}-rpm"
RPM_NAME="skrynja-1-pr${PR}.rpm"
DOWNLOAD_URL="https://raw.githubusercontent.com/${REPO}/${BRANCH}/${RPM_NAME}"

if [ -z "$PR" ]; then
    echo "Brug: sh test_pr.sh <pr-nummer>"
    echo "Eksempel: sh test_pr.sh 2"
    exit 1
fi

echo "=== Skrynja-1 PR-test ==="
echo "PR: #${PR}"
echo "URL: ${DOWNLOAD_URL}"

TMP_DIR="/tmp/skrynja-1-pr-test"
rm -rf "${TMP_DIR}"
mkdir -p "${TMP_DIR}"
cd "${TMP_DIR}"

if ! curl -sfL -o "${RPM_NAME}" "${DOWNLOAD_URL}"; then
    echo "FEJL: Kunne ikke hente RPM. Har PR-bygget (pr_byg.yml) kørt færdigt?" >&2
    echo "Prøvede: ${DOWNLOAD_URL}" >&2
    rm -rf "${TMP_DIR}"
    exit 1
fi
echo "Hentet: ${RPM_NAME} ($(du -h "${RPM_NAME}" | cut -f1))"

devel-su pkcon install-local --allow-reinstall "${TMP_DIR}/${RPM_NAME}"

rm -rf "${TMP_DIR}"
echo "=== Færdig: Skrynja-1 PR #${PR} (aarch64) er installeret ==="
