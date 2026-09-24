# Optional macOS release signing

macOS release signing is disabled unless the repository variable
`MACOS_SIGNING_ENABLED` is `true`. Unset or `false` preserves the existing
ad-hoc-signed macOS artifacts. Other values fail configuration validation.
Normal builds and pull requests never receive signing credentials.

## Enable

1. Create a `macos-release` environment. Restrict it to release tags (`v*`)
   and require a trusted release reviewer before exposing credentials.
2. Add the environment secrets and variables below, using a Developer ID
   Application identity controlled by the release maintainer.
3. Set the **repository** variable `MACOS_SIGNING_ENABLED=true`.
4. Tag a commit with a successful `Build` push run from `main` or `dev`.
   Approve the signing deployment when requested.

Environment secrets:

| Name | Value |
| --- | --- |
| `MACOS_CERTIFICATE_P12_BASE64` | Base64 of a password-protected, macOS Keychain-compatible PKCS12 containing the certificate and private key |
| `MACOS_CERTIFICATE_PASSWORD` | PKCS12 password |
| `APPLE_APP_SPECIFIC_PASSWORD` | Dedicated Apple app-specific password for notarization |

Environment variables:

| Name | Value |
| --- | --- |
| `MACOS_SIGNING_IDENTITY` | Certificate's 40-character SHA-1 fingerprint |
| `APPLE_TEAM_ID` | Ten-character Apple Developer Team ID |
| `APPLE_ID` | Apple account login used for notarization |

The signing identity and Apple credential are not restricted to one app by
Apple. Keep them in the protected environment, review changes to credentialed
workflows, and reassess access when release maintainers change. Do not reuse
another distributor's signing credentials.

## Release behavior

Signing reuses the exact selected CI installer; it never rebuilds with secrets.
The library, loader and app receive timestamped Developer ID signatures, then
the final signed DMG is submitted once. The job checks Apple's ticket coverage
for both architectures, staples the DMG, and verifies the copied-out app with
Gatekeeper. This follows Apple's
[outermost-container guidance](https://developer.apple.com/forums/thread/125512).
The standalone library archive contains the same signed library; standalone
dylibs cannot carry a staple and may need Apple's online ticket lookup.

Missing credentials, rejected submissions or failed verification stop the
release; they never fall back to ad-hoc artifacts when signing is enabled.
The release includes `macos-provenance.json` with source/build identity,
submitted/final hashes and the notarization ID.

Apple processing has a bounded 20-minute wait. On an ordinary failure, the
workflow retains the exact submitted payload, provenance and notarization
evidence for 30 days. Inspect the existing submission before retrying: a rerun
starts a new submission. Recovery is manual; runner loss can prevent artifact
retention. Temporary signing keys are removed when the script exits.

Test the final downloaded DMG before enabling this for routine releases. A
fresh Mac or VM is needed to establish offline first-open behavior without
cached tickets: download, disconnect, mount, copy the app out, eject and open.
Reconnect before testing the online game. Online CI assessment alone does not
establish offline behavior.
