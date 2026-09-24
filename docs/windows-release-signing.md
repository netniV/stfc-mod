# Optional Windows release signing

Windows release signing is disabled unless the repository variable
`WINDOWS_SIGNING_ENABLED` is `true`. Unset or `false` preserves the existing
unsigned Windows artifacts. Other values fail configuration validation.
Normal builds and pull requests never receive signing access.

## Enable

1. Use an Azure Artifact Signing account with a **Public Trust** certificate
   profile. The account can belong to the maintainer or to an account owner
   who explicitly authorizes signing upstream releases under their identity.
2. Create a `windows-release` GitHub environment. Restrict it to release tags
   (`v*`) and require a trusted release reviewer.
3. The account owner configures a dedicated Azure identity with a GitHub OIDC federated credential for
   `repo:OWNER/REPOSITORY:environment:windows-release`, audience
   `api://AzureADTokenExchange`, issuer `https://token.actions.githubusercontent.com`.
   Grant **Artifact Signing Certificate Profile Signer** on the intended
   certificate profile, not the entire subscription. See Microsoft's
   [role setup](https://learn.microsoft.com/en-us/azure/artifact-signing/tutorial-assign-roles).
4. Set the environment variables below, then set the **repository** variable
   `WINDOWS_SIGNING_ENABLED=true`.
5. Tag a commit with a successful `Build` push run from `main` or `dev`.
   Approve the signing deployment when requested.

| Environment variable | Value |
| --- | --- |
| `AZURE_CLIENT_ID` | Azure identity client ID |
| `AZURE_TENANT_ID` | Azure tenant ID |
| `AZURE_SUBSCRIPTION_ID` | Subscription containing the signing account |
| `AZURE_TRUSTED_SIGNING_ENDPOINT` | Regional Artifact Signing endpoint |
| `AZURE_CODE_SIGNING_ACCOUNT_NAME` | Artifact Signing account name |
| `AZURE_CERTIFICATE_PROFILE_NAME` | Public Trust certificate profile name |
| `WIN_PUBLISHER_NAME` | Expected certificate publisher, exactly as displayed in its simple name |

OIDC avoids storing a client secret or exporting a private key. The signing
job alone receives `id-token: write`; build and publication jobs do not.
Review changes to credentialed workflows and reassess environment/Azure access
when release maintainers change. The profile role permits signing other code
too; it is not a restriction to this particular DLL.

For delegated signing, use a separate upstream certificate profile and OIDC
identity instead of sharing the account owner's downstream release identity.
The account owner can remove the upstream identity's role assignment or
federated credential to stop future access independently. The repository
maintainer does not need the owner's Azure login or subscription-wide access.

## Release behavior

The signing job downloads the exact selected CI DLL and checks its embedded
commit/build identity before signing. Azure applies a SHA-256 Authenticode
signature and RFC3161 timestamp. Verification requires a valid signature,
timestamp, configured publisher, supported RSA key and Public Trust/code-signing
certificate usages.

Only the verified DLL is packaged into the existing Windows ZIPs and archive.
The release also includes `provenance.json` recording build source, unsigned
and signed hashes, signer and certificate thumbprint.

Missing configuration, signing or verification failure stops the release;
it never silently falls back to unsigned files when signing is enabled.
Configure and validate the authorized signing identity before enabling routine
signed releases. A valid signature does not guarantee immediate SmartScreen
reputation for every new download.
