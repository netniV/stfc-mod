#!/usr/bin/env bash
# Re-sign the exact CI installer selected by the release workflow. Never rebuild with credentials.
set -euo pipefail
umask 077

: "${RUNNER_TEMP:?}" "${MACOS_CERTIFICATE_P12_BASE64:?}" "${MACOS_CERTIFICATE_PASSWORD:?}"
: "${APPLE_APP_SPECIFIC_PASSWORD:?}" "${APPLE_ID:?}" "${APPLE_TEAM_ID:?}"
: "${MACOS_SIGNING_IDENTITY:?}" "${SOURCE_SHA:?}" "${BUILD_RUN_ID:?}"
[[ "$SOURCE_SHA" =~ ^[0-9a-f]{40}$ && "$BUILD_RUN_ID" =~ ^[1-9][0-9]*$ ]]
[[ "$APPLE_TEAM_ID" =~ ^[A-Z0-9]{10}$ && "$MACOS_SIGNING_IDENTITY" =~ ^[A-Fa-f0-9]{40}$ ]]
command -v zstd >/dev/null

work=$(mktemp -d "$RUNNER_TEMP/stfc-macos.XXXXXX")
keychain="$RUNNER_TEMP/stfc-signing.keychain-db"
original_keychains=()
while IFS= read -r entry; do
  original_keychains+=("$entry")
done < <(security list-keychains -d user | sed 's/^[[:space:]]*"//; s/"[[:space:]]*$//')
mount="$work/mount"
mkdir -p "$mount" signed-macos macos-notarization-evidence macos-notarization-payloads
cleanup() {
  hdiutil detach "$mount" -quiet 2>/dev/null || true
  security list-keychains -d user -s "${original_keychains[@]}" || true
  security delete-keychain "$keychain" 2>/dev/null || true
  # mktemp above owns this directory; it contains no user files.
  rm -rf "$work"
}
trap cleanup EXIT

printf '%s' "$MACOS_CERTIFICATE_P12_BASE64" | base64 --decode > "$work/identity.p12"
keychain_password=$(openssl rand -hex 32)
security create-keychain -p "$keychain_password" "$keychain"
security set-keychain-settings -lut 3600 "$keychain"
security unlock-keychain -p "$keychain_password" "$keychain"
security list-keychains -d user -s "$keychain" "${original_keychains[@]}"
security import "$work/identity.p12" -k "$keychain" -P "$MACOS_CERTIFICATE_PASSWORD" -T /usr/bin/codesign
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain" >/dev/null
security find-identity -v -p codesigning "$keychain" \
  | tee macos-notarization-evidence/signing-identities.txt \
  | grep -F "$MACOS_SIGNING_IDENTITY" >/dev/null
xcrun notarytool store-credentials stfc-notary --keychain "$keychain" \
  --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_SPECIFIC_PASSWORD"
rm "$work/identity.p12"
unset MACOS_CERTIFICATE_P12_BASE64 MACOS_CERTIFICATE_PASSWORD APPLE_APP_SPECIFIC_PASSWORD keychain_password

input=unsigned-macos/stfc-community-mod-installer.dmg
input_hash=$(shasum -a 256 "$input" | awk '{print $1}')
hdiutil attach "$input" -readonly -nobrowse -mountpoint "$mount" -quiet
# Preserve the existing installer artwork, layout and Applications link.
ditto "$mount" "$work/dmg-root"
hdiutil detach "$mount" -quiet
app="$work/dmg-root/STFC Community Mod.app"
loader="$app/Contents/stfc-community-mod-loader"
library="$app/Contents/libstfc-community-mod.dylib"
launcher="$app/Contents/MacOS/macOSLauncher"
for binary in "$library" "$loader" "$launcher"; do
  test -f "$binary"
  lipo "$binary" -verify_arch arm64 x86_64
done
unsigned_library_hash=$(shasum -a 256 "$library" | awk '{print $1}')

# Sign nested code explicitly before sealing the outer bundle. The library runs
# inside the game's process and uses the host's entitlements, not the launcher's.
codesign --force --timestamp --options runtime --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" "$library"
codesign --force --timestamp --options runtime --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" "$loader"
codesign --force --timestamp --options runtime --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" \
  --entitlements macos-launcher/src/macOSLauncher.entitlements "$app"
for binary in "$library" "$loader" "$app"; do
  codesign --verify --strict --all-architectures --verbose=2 "$binary"
  for arch in arm64 x86_64; do
    details=$(codesign --display --verbose=4 --arch "$arch" "$binary" 2>&1)
    grep -Fx "TeamIdentifier=$APPLE_TEAM_ID" <<< "$details" >/dev/null
    grep -F 'Authority=Developer ID Application:' <<< "$details" >/dev/null
    grep -E '^Timestamp=.+$' <<< "$details" >/dev/null
    grep -F '(runtime)' <<< "$details" >/dev/null
  done
done
codesign --verify --deep --strict --all-architectures "$app"

notarize() {
  local file="$1" name="$2" result id status payload payload_hash
  result="macos-notarization-evidence/$name-submission.json"
  # Retain the exact signed upload, outside the temporary signing directory.
  # A delayed verdict must not require rebuilding or re-signing accepted code.
  payload="macos-notarization-payloads/$(basename "$file")"
  cp "$file" "$payload"
  payload_hash=$(shasum -a 256 "$payload" | awk '{print $1}')
  jq -n --arg source "$SOURCE_SHA" --arg build "$BUILD_RUN_ID" \
    --arg input "$input_hash" --arg payload "$payload_hash" \
    --arg workflow "$GITHUB_SHA" --arg filename "$(basename "$file")" \
    '{sourceCommit:$source,buildRunId:$build,inputDmgSha256:$input,
      payloadSha256:$payload,filename:$filename,signingWorkflowCommit:$workflow}' \
    > "macos-notarization-payloads/$name-provenance.json"
  echo "Submitting $name ($payload_hash) to Apple"
  # Submit separately so the ID is saved before the potentially long wait.
  # Never retry submission automatically: inspect history after upload errors.
  xcrun notarytool submit "$payload" --keychain-profile stfc-notary --keychain "$keychain" \
    --output-format json > "$result"
  id=$(jq -r '.id // empty' "$result")
  [[ "$id" =~ ^[[:xdigit:]]{8}-[[:xdigit:]]{4}-[[:xdigit:]]{4}-[[:xdigit:]]{4}-[[:xdigit:]]{12}$ ]]
  cp "$result" "macos-notarization-payloads/$name-submission.json"
  echo "Apple $name submission: $id; waiting up to 20 minutes"
  xcrun notarytool wait "$id" --keychain-profile stfc-notary --keychain "$keychain" \
    --timeout 20m --output-format json > "macos-notarization-evidence/$name-wait.json" || true
  # Keep the original submission receipt even if this status request fails.
  xcrun notarytool info "$id" --keychain-profile stfc-notary --keychain "$keychain" \
    --output-format json > "macos-notarization-evidence/$name-info.json"
  cp "macos-notarization-evidence/$name-info.json" "$result"
  status=$(jq -r '.status // empty' "$result")
  echo "Apple $name status: $status"
  xcrun notarytool log "$id" --keychain-profile stfc-notary --keychain "$keychain" \
    "macos-notarization-evidence/$name-log.json" || true
  if [[ "$status" != Accepted ]]; then
    echo "::error::Notarization $name status: $status; submission: $id. See evidence artifact."
    return 1
  fi
}

# Submit only the outermost container; Apple's ticket covers its nested code.
# Do not repack or re-sign it after acceptance. Stapling preserves its signature.
signed_library_hash=$(shasum -a 256 "$library" | awk '{print $1}')
output=signed-macos/stfc-community-mod-installer.dmg
hdiutil create -quiet -volname 'STFC Community Mod Installer' -srcfolder "$work/dmg-root" -format UDZO "$output"
codesign --timestamp --keychain "$keychain" --sign "$MACOS_SIGNING_IDENTITY" "$output"
notarize "$output" dmg
submitted_dmg_hash=$(shasum -a 256 "$output" | awk '{print $1}')
submission_id=$(jq -r .id macos-notarization-evidence/dmg-submission.json)
ticket_log=macos-notarization-evidence/dmg-log.json
jq -e --arg id "$submission_id" --arg hash "$submitted_dmg_hash" \
  '.jobId == $id and .status == "Accepted" and .sha256 == $hash' "$ticket_log" >/dev/null

# Acceptance must cover the exact signed code we ship, including both slices.
verify_ticket() {
  local binary="$1" path="$2" arch="${3:-}" details cdhash
  if [[ -n "$arch" ]]; then
    details=$(codesign --display --verbose=4 --arch "$arch" "$binary" 2>&1)
  else
    details=$(codesign --display --verbose=4 "$binary" 2>&1)
  fi
  cdhash=$(sed -n 's/^CDHash=//p' <<< "$details")
  [[ "$cdhash" =~ ^[a-f0-9]{40}$ ]]
  jq -e --arg path "$path" --arg arch "$arch" --arg hash "$cdhash" \
    'any(.ticketContents[]; .path == $path and (.arch // "") == $arch and .cdhash == $hash)' \
    "$ticket_log" >/dev/null
  echo "Apple ticket covers $path ${arch:-container}: $cdhash"
}
verify_ticket "$output" "$(basename "$output")"
for binary in "$app" "$loader" "$library"; do
  ticket_path="$(basename "$output")/${binary#"$work/dmg-root/"}"
  for arch in arm64 x86_64; do
    verify_ticket "$binary" "$ticket_path" "$arch"
  done
done
xcrun stapler staple "$output"
xcrun stapler validate "$output"
codesign --verify --strict "$output"
spctl --assess --type open --context context:primary-signature --verbose=2 "$output"

# Verify the app after copying it out of the actual shipped image. Gatekeeper
# ingests the stapled container ticket; this online runner isn't an offline test.
hdiutil attach "$output" -readonly -nobrowse -mountpoint "$mount" -quiet
installed_app="$work/installed/STFC Community Mod.app"
ditto "$mount/STFC Community Mod.app" "$installed_app"
hdiutil detach "$mount" -quiet
codesign --verify --deep --strict --all-architectures "$installed_app"
spctl --assess --type execute --verbose=2 "$installed_app"
[[ "$(shasum -a 256 "$installed_app/Contents/libstfc-community-mod.dylib" | awk '{print $1}')" == "$signed_library_hash" ]]

# The standalone archive contains the exact library accepted in the DMG.
# Dylibs cannot carry a stapled ticket; separate downloads use online lookup.
archive=signed-macos/stfc-community-mod-macos-universal.tar.zst
tar -cf - -C "$installed_app/Contents" libstfc-community-mod.dylib | zstd -15 -T0 -o "$archive"
shasum -a 256 "$archive" | awk '{print $1}' > "$archive.sha256"
dmg_hash=$(shasum -a 256 "$output" | awk '{print $1}')
jq -n --arg source "$SOURCE_SHA" --arg build "$BUILD_RUN_ID" --arg team "$APPLE_TEAM_ID" \
  --arg signer "$MACOS_SIGNING_IDENTITY" --arg input "$input_hash" \
  --arg unsigned "$unsigned_library_hash" --arg signed "$signed_library_hash" --arg dmg "$dmg_hash" \
  --arg submittedDmg "$submitted_dmg_hash" \
  --arg workflow "$GITHUB_SHA" --arg url "$GITHUB_SERVER_URL/$GITHUB_REPOSITORY/actions/runs/$GITHUB_RUN_ID" \
  --slurpfile dmgTicket macos-notarization-evidence/dmg-submission.json \
  '{sourceCommit:$source,buildRunId:$build,teamId:$team,signingIdentity:$signer,
    inputDmgSha256:$input,unsignedLibrarySha256:$unsigned,signedLibrarySha256:$signed,
    dmgSha256:$dmg,signingWorkflowCommit:$workflow,signingRunUrl:$url,
    notarizationContainer:"dmg",submittedDmgSha256:$submittedDmg,
    dmgNotarization:$dmgTicket[0]}' > signed-macos/macos-provenance.json
