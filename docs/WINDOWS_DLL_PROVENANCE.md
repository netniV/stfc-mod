# Windows DLL provenance

Windows builds publish a `stfc-identity-v1` marker in the DLL's standard `Comments` version resource. The marker is
minimal, descriptive, and self-declared. It helps identify a build, but it is not evidence of authenticity and must
not be used as an authorization or trust decision by itself.

For protected `main` and `dev` pushes in `netniV/stfc-mod`, the upstream Build workflow creates a GitHub/Sigstore
build-provenance attestation for the exact `version.dll` bytes after verifying the embedded marker. Pull requests,
fork runs, unprotected refs, tags, and other repositories do not create authoritative attestations.

After downloading and extracting an attested DLL, verify its byte integrity and expected upstream workflow identity
with a current GitHub CLI:

```shell
gh attestation verify version.dll \
  --repo netniV/stfc-mod \
  --signer-workflow netniV/stfc-mod/.github/workflows/ci.yaml
```

Successful verification establishes that the exact file digest is covered by an attestation issued for the named
repository and signer workflow, including source commit provenance. It does not establish that the source,
dependencies, build environment, or resulting behavior are free from malicious or unsafe behavior. It also does not,
by itself, authorize the artifact as an official release.

The first authoritative upstream attestation can exist only after the attestation workflow is merged and then runs on
an eligible protected upstream push. Existing releases are unattested. Downstream consumers, including STFC Mod
Bridge, must continue to use exact reviewed file size and SHA-256 allowlists until an attested upstream artifact is
actually available and their attestation policy is separately reviewed.
