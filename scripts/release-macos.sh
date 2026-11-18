#!/usr/bin/env bash
# Builds and tests the app, signs it with a Developer ID, has Apple notarise the disk image and
# staples the ticket to it. With --upload, it also attaches the disk image to the GitHub release
# tagged v<version>, replacing the one there.
#
#   bash scripts/release-macos.sh <version> [--upload]
#
# Locally it needs nothing set: it uses the Developer ID Application identity found in the
# keychain and the notarytool keychain profile "mac-apps". Make that profile once, from an App
# Store Connect API key:
#
#   xcrun notarytool store-credentials mac-apps --key AuthKey_<key id>.p8 --key-id <key id> --issuer <issuer id>
#
# These environment variables override the defaults:
#   DAC_SIGN_IDENTITY     a "Developer ID Application: Name (TEAMID)" identity in the keychain.
#   DAC_NOTARY_KEY        the path of an App Store Connect API key's .p8 file,
#   DAC_NOTARY_KEY_ID     the key's ID and
#   DAC_NOTARY_ISSUER     its issuer ID. The release workflow sets these three.
#   DAC_NOTARY_PROFILE    the notarytool keychain profile, when DAC_NOTARY_KEY is not set.

set -euo pipefail

usage() {
    echo "usage: bash scripts/release-macos.sh <version> [--upload]" >&2
    exit 2
}

[ $# -ge 1 ] || usage
VERSION=${1#v}
UPLOAD=0
if [ $# -ge 2 ]; then
    [ "$2" = "--upload" ] || usage
    UPLOAD=1
fi
if ! [[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "release-macos.sh: version '$1' is not X.Y.Z or vX.Y.Z" >&2
    exit 2
fi

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build/macos-release"
DMG="$BUILD/dev-asset-cleaner-$VERSION-macos.dmg"

IDENTITY=${DAC_SIGN_IDENTITY:-}
if [ -z "$IDENTITY" ]; then
    IDENTITY=$(security find-identity -v -p codesigning \
        | sed -n 's/.*"\(Developer ID Application: .*\)"$/\1/p' | head -1)
fi
NOTARY_ARGS=()
if [ -n "${DAC_NOTARY_KEY:-}" ]; then
    NOTARY_ARGS=(--key "$DAC_NOTARY_KEY" --key-id "${DAC_NOTARY_KEY_ID:?needs DAC_NOTARY_KEY_ID}"
        --issuer "${DAC_NOTARY_ISSUER:?needs DAC_NOTARY_ISSUER}")
else
    NOTARY_ARGS=(--keychain-profile "${DAC_NOTARY_PROFILE:-mac-apps}")
fi

# Check the signing settings before the build, so a mistake costs a second and not a build.
if [ -z "$IDENTITY" ]; then
    echo "release-macos.sh: the keychain has no Developer ID Application identity." >&2
    exit 1
fi
if ! security find-identity -v -p codesigning | grep -qF "$IDENTITY"; then
    echo "release-macos.sh: the keychain has no signing identity named:" >&2
    echo "  $IDENTITY" >&2
    echo "'security find-identity -v -p codesigning' lists the ones it has." >&2
    exit 1
fi

sh "$ROOT/scripts/setup.sh"

# A separate build folder, so the Developer ID setting does not reach everyday builds.
cmake --preset macos -B "$BUILD" \
    "-DDAC_VERSION=$VERSION" "-DDAC_CODESIGN_IDENTITY=$IDENTITY"
cmake --build "$BUILD" --config Release
ctest --test-dir "$BUILD" -C Release --output-on-failure

rm -f "$DMG"
(cd "$BUILD" && cpack -C Release)
codesign --force --timestamp --sign "$IDENTITY" "$DMG"

# One submission covers the disk image and the app inside it. Apple's answer takes a few
# minutes. notarytool can exit with 0 for a rejected submission, so read the status it reports.
echo "Notarising $DMG"
OUT=$(mktemp)
trap 'rm -f "$OUT"' EXIT
if ! xcrun notarytool submit "$DMG" "${NOTARY_ARGS[@]}" --wait --output-format json > "$OUT" \
    || ! grep -q '"status" *: *"Accepted"' "$OUT"; then
    cat "$OUT" >&2
    ID=$(sed -n 's/.*"id" *: *"\([^"]*\)".*/\1/p' "$OUT" | head -1)
    if [ -n "$ID" ]; then
        echo "Apple's log of the submission:" >&2
        xcrun notarytool log "$ID" "${NOTARY_ARGS[@]}" >&2 || true
    fi
    echo "release-macos.sh: notarisation failed." >&2
    exit 1
fi
cat "$OUT"

# The stapled ticket lets Gatekeeper accept the disk image on a Mac without a network
# connection. Apple's ticket server sometimes drops the connection, so try a few times.
ATTEMPT=1
until xcrun stapler staple "$DMG"; do
    if [ "$ATTEMPT" -ge 5 ]; then
        echo "release-macos.sh: stapling $DMG failed $ATTEMPT times." >&2
        exit 1
    fi
    ATTEMPT=$((ATTEMPT + 1))
    echo "Stapling failed; trying again in 30 seconds." >&2
    sleep 30
done
spctl --assess --type open --context context:primary-signature --verbose "$DMG"

echo "Notarised disk image: $DMG"

if [ "$UPLOAD" = 1 ]; then
    gh release upload "v$VERSION" "$DMG" --clobber
    echo "Attached to release v$VERSION"
fi
