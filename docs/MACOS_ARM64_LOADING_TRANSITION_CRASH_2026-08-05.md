# macOS ARM64 loading-transition crash (2026-08-05)

## Conclusion

The crash is the same SPUD ARM64 register-indirect branch relocation failure addressed by PR #203. It is not a
regression of that repair, a newly uncovered generated-stub form, or evidence that the reporter loaded an old mod.

The reporter did load the intended artifact from Actions run `30804070579`: its ARM64 UUID is
`B7769726-C9C5-38F6-9CC6-D79877F1D794`. The important distinction is that the run's source commit,
`1b0383d4fde2ed1bbe8e33f88d1083d95a969444`, does **not** contain PR #203's relocation repair. The UUID proves the
current-beta artifact identity; it does not prove inclusion of an unmerged fix.

The strongest hook-owner attribution is `il2cpp_unity_liveness_finalize`, installed by `InstallObjectTrackers()`.
The generated-code mechanism is confirmed from the crash bytes. Owner attribution is a strong inference rather than
an independent symbolication of the GameAssembly entry point because the exact `GameAssembly.dylib` was not attached.

## Reporter-visible impact

- Game: STFC `1.000.51211` (`100051211`)
- Host: Apple M4 Pro, macOS `26.5.2` (`25F84`)
- Transition: loading reaches approximately 100%, the splash window shrinks and closes, and macOS presents a quit
  report.
- Signal: `EXC_BAD_INSTRUCTION (SIGILL)` on the main thread during splash-scene unload.

This is expected to remain reproducible in artifacts built from `dev` at `1b0383d` while the object tracker is enabled.

## Artifact and source identity

GitHub Actions metadata for run `30804070579` records:

- workflow: `.github/workflows/ci.yaml`, run 748, successful push build;
- head branch: `dev`;
- head SHA: `1b0383d4fde2ed1bbe8e33f88d1083d95a969444`;
- installer artifact `8852330317`, GitHub digest
  `sha256:8275cdace15b99d91b6758fd3fa6ba982fc9a15aaf57629e70e00b2fa288ac32`;
- universal artifact `8852331027`, GitHub digest
  `sha256:7f049aab543420e10e4c47abae61f37de74acedb7327c0595a31309681d29ed3`.

The downloaded universal payload was verified independently:

- `stfc-community-mod-macos-universal.tar.zst` SHA-256:
  `78be8d3cb7fe15072f0e9261a78beedc1de74935619c7a84f74970a45accadf4`;
- its companion checksum file contains the same value;
- extracted universal `libstfc-community-mod.dylib` SHA-256:
  `e31e5000d0764a924b44059ff3d7a5f14b34bb931b4187ad22087ba8f5e456ec`;
- ARM64 Mach-O UUID: `B7769726-C9C5-38F6-9CC6-D79877F1D794`.

The supplied report text used for this analysis has SHA-256
`f313467e5207efb64f34b001537c45701c841e86a22fa02d2a558fe32a402b1f`.

The exact-run workflow checks out the pushed SHA directly and builds both macOS architectures from that checkout. Git
history shows that PR #203's main relocation commit, `a0b3c0633ec8df012ebd51383a7f7087cca31701`, is not an ancestor
of `1b0383d`. At the time of investigation PR #203 is open on
`fix/macos-loading-screen-transition-crash`; its repair commits exist only on that branch.

The exact source and extracted binary both agree with this ancestry result:

- `aarch64.cc` reads the last branch operand's union member as `.imm` without first requiring
  `AARCH64_OP_IMM`;
- `relocators.cc` sends all jump/call-group instructions to `branch_relocator`, including register-indirect branches;
- the repaired object-tracker confirmation string is absent from the exact artifact;
- release defaults enable `patches.objecttracker`.

## Follow-up Actions run audit

### PR #203 run `30892368903`

The last run on PR #203 checked out the synthetic pull-request merge commit
`b3ea74001b84c2f9f7f7cd3b1feac615062dddcc` (`63ddee3` merged into `1b0383d`). Both repair commits
`a0b3c06` and `8426065` are ancestors of that checkout. Its ARM64 build completed successfully, so the ARM64 binary
compiled during the job contained the relocation repair and restored liveness hook.

The run nevertheless cannot supply a replacement macOS binary. During the subsequent x86_64 configuration, package
setup reported failures involving OpenSSL, SPUD, and protobuf; the job made no further visible progress and was
cancelled after approximately six hours. The packaging and artifact-upload steps were skipped. The run's only
retained artifact is `8886187674` (`stfc-community-mod`), the Windows artifact. There is no macOS installer or
universal artifact from this run.

### Combined-fix fork run `29790403795`

The successful `Guffawaffle/stfc-mod` run used head
`2cef0c1e004af4c8000028d99e9cb765db8d21e9` on `test/macos-combined-runtime-fixes`. Both `a0b3c06` and `8426065` are
ancestors of that commit, and the exact extracted binary contains the repair's macOS ARM64 object-tracker log marker.
It therefore **does include the PR #203 fix**.

Verified macOS outputs from that run:

- universal artifact `8480517564`, GitHub digest
  `sha256:23f230374a0cb5c43aeb73220a6ad80ca353c3ad28cebd34ad30c83b20e6fe5e`;
- installer artifact `8480516555`, GitHub digest
  `sha256:e74851c92f99c9bc9a4ff033182cbe8ec0b207d3884078eba2941a8fd35dd7dd`;
- installer DMG SHA-256: `bad8a502e1552e7c7135024419ae29558562a7d38140a105d1c89748d43538ac`;
- universal payload SHA-256: `e79b55e3f8a9a6f568289f503c6d3a155b32723ff7dbbbd1401a19abd732f519`, matching its
  companion checksum file;
- extracted universal dylib SHA-256: `47bc082d698b5a1064d083054a3a5d0ec35e5ed03248ca2182348320b60f3039`;
- ARM64 Mach-O UUID: `5590236E-F2F4-351E-A059-D0DACAF59075`.

The installer DMG was also unpacked directly. Both its standalone ARM64 dylib and the ARM64 slice of the universal
dylib inside `STFC Community Mod.app` have UUID `5590236E-F2F4-351E-A059-D0DACAF59075` and contain the repaired
object-tracker log marker. Thus the installable artifact itself, not merely the run's source or companion archive,
contains the fix.

This fork artifact is a useful focused A/B test, but it is not a current-`dev` plus-fix build. Its merge base with
`1b0383d` is `0d76350`; it does not contain the later `dev` changes from PRs #209, #214, #216, and #193. Its UUID also
differs from the crashing artifact's `B7769726-C9C5-38F6-9CC6-D79877F1D794`, so the supplied crash did not originate
from this fixed fork artifact.

### Current-`dev` replacement run `31001017600`

PR #203 was refreshed at `bc15dcf8b73f1e0783051b6733b5d685e7c47450`. The branch contains current `dev` head `1b0383d`,
both ARM64 repair commits, and a macOS-only libcurl `8.11.0` pin which avoids an August 4 XMake repository change
that made newer Apple libcurl builds pull the failing OpenSSL 3 x86_64 cross-build.

The macOS job completed successfully, including ARM64 and x86_64 configuration/build, universal packaging, signing,
installer creation, and both uploads. Verified output:

- installer artifact `8928730322`, GitHub digest
  `sha256:8da4e0c4720fbc338f76f071f203b8feb696ebb7e8c6c3d9d92e707edcb43ff7`;
- universal artifact `8928730990`, GitHub digest
  `sha256:234c19e244a9e1898b11743dc8b5ce12c6c051d2db06e7852b41c26d5ac9c2b8`;
- installer DMG SHA-256: `3d64511c2ebcf66c30e87a07f6e2528be4d2a82be1242010e36103451a68895e`;
- ARM64 Mach-O UUID: `ADBAFC7A-C32F-315C-A42A-C231410DEFA1`.

The downloaded DMG was unpacked directly. Its standalone ARM64 dylib and the ARM64 slice of the app's universal
dylib both have the UUID above and contain the repaired object-tracker log marker. This is the first verified
installable artifact in the investigation that combines PR #203 with all changes in `dev` at `1b0383d`.

### Reporter runtime validation

The original reporter installed the equivalent successful fork-run installer from run `31001014060`, artifact
`8928835656`, built from the same `bc15dcf` source. Its DMG SHA-256 is
`21860b2f8112963297a65aa183e4ce86c7e274b771b5053ab6b47fdf37dbd6be`; its ARM64 UUID is
`ADBAFC7A-C32F-315C-A42A-C231410DEFA1`, and the repair marker is present.

The reporter confirmed that STFC loaded successfully past the splash-to-game-UI transition which had reproducibly
crashed with UUID `B7769726-C9C5-38F6-9CC6-D79877F1D794`. This same-reporter, same-transition result validates the
PR #203 relocation repair in combination with current `dev`. Spocks Club synchronization validation was still
pending while the reporter added their mod credentials.

## Generated trampoline symbolication

The crash report places the PC at `0x11efe401c`. This is outside every listed Mach-O image and is exactly `0x1c` bytes
into the 16 KiB-aligned allocation beginning at `0x11efe4000`, consistent with SPUD's one-`mmap`-per-trampoline
executable allocations.

The report's `instructionByteStream.atPC` decodes as follows:

| Address | Word | Meaning |
| --- | --- | --- |
| `0x11efe401c` | `0x00000014` | `UDF #0x14` (the trapping, corrupted instruction) |
| `0x11efe4020` | `0x58000051` | `LDR X17, literal` |
| `0x11efe4024` | `0xD61F0220` | `BR X17` |
| `0x11efe4028` | `0x0000000129154384` | GameAssembly return address literal |
| `0x11efe4030` | `0x58000050` | `LDR X16, literal` |
| `0x11efe4034` | `0xD61F0200` | `BR X16` |
| `0x11efe4038` | `0x00000000000000F7` | bogus branch target / Capstone `X9` register ID |

The `LDR X17; BR X17; <return address>` sequence is SPUD ARM64 `create_trampoline()`'s tail. The following
`LDR X16; BR X16; <literal>` sequence is `branch_relocator`'s relocation-data stub. This layout proves that the PC is
inside a generated SPUD trampoline rather than an unsymbolicated section of the mod dylib.

The terminal literal `0xF7` is the signature of the pre-#203 bug. Capstone describes an indirect `BLR X9` target as a
register operand. The old code reads the operand union as an immediate, observes the register ID as if it were an
address, classifies the position-independent instruction as a relative branch, and then runs it through a relocation
path which supports only immediate `B`, `BL`, `CBZ`/`CBNZ`, and `TBZ`/`TBNZ` encodings. In this crash it rewrote the
copied instruction to `0x00000014`, which ARM64 executes as `UDF #0x14` and raises `SIGILL`.

The embedded return address is `0x129154384`, or GameAssembly image offset `0x6ec384` from the report's
`0x128a68000` image base.

## Hook ownership

The likely owner is the object tracker's `il2cpp_unity_liveness_finalize` detour:

1. The exact beta unconditionally installs this detour on macOS ARM64 from
   `mods/src/patches/parts/object_tracker.cc`.
2. Object tracking is enabled by default in the exact beta's release configuration.
3. PR #203's first mitigation explicitly disabled this detour because its current ARM64 prologue generated a SPUD
   trampoline that entered corrupted relocation code during loading-scene unload.
4. The later PR #203 repair specifically handles the register-indirect `BR`/`BLR` operand form exhibited by the
   decoded bytes above, then restores the same liveness hook.
5. The runtime stack includes `SceneManager_UnloadScene` and `DoLoad.MoveNext`, the lifecycle boundary at which Unity
   runs liveness/finalization work and at which the earlier failure was reproduced.

An exact `GameAssembly.dylib` or a runtime trampoline registry would independently turn image offset `0x6ec384` into
an exported-symbol ownership proof. Neither was present in the supplied evidence. This limitation does not affect the
relocation root-cause conclusion, because the generated bytes contain the complete pre-fix failure signature.

## Assessment of the four proposed causes

1. **Regression in repaired ARM64 trampoline relocation: ruled out.** The artifact predates inclusion of the repair;
   there is no repaired code here to have regressed.
2. **Another register-indirect branch or generated-stub form not covered by #203: ruled out.** The `0xF7` literal and
   the two SPUD stub sequences identify the same `X9` register-operand misclassification covered by #203.
3. **A different scene-unload/UI-release hook: unlikely.** The liveness-finalize hook is the direct historical and
   lifecycle match. Exact owner symbolication remains the one evidence gap noted above.
4. **Interaction introduced after the transition fix: ruled out for this artifact.** The transition fix was never in
   this artifact. Later `dev` work may affect timing, but it is not needed to explain the deterministic illegal opcode.

## Proposed fix

Merge PR #203 into `dev`, then rebuild and redistribute the beta. The relevant repair is already implemented there:

- `a0b3c0633ec8df012ebd51383a7f7087cca31701` preserves register-indirect ARM64 branches and adds defensive
  immediate-operand checks;
- `8426065fbfab85046a7090955719fcff58596b16` restores the liveness hook after the relocation repair and leaves a
  visible macOS ARM64 confirmation log.

The emergency mitigation, if the relocation repair cannot ship immediately, is the earlier PR commit
`c9c5431683d7293ad9d9a2925a4d76287ed0e890`, which disables only the liveness-finalize detour on Apple ARM64 while
retaining constructor, `OnDestroy`, and GC-finalizer tracking.

Do not treat reinstalling the `1b0383d` artifact as a remediation: that artifact is internally consistent but contains
the known-bad relocator.

## Validation for the replacement artifact

1. Verify the replacement workflow head contains `a0b3c06` (or an equivalent operand-type guard) before using its UUID
   as provenance evidence.
2. Verify the universal artifact checksum and the installed ARM64 Mach-O UUID.
3. Start STFC on Apple Silicon with object tracking enabled and confirm the log contains the PR #203 macOS ARM64
   liveness-hook marker.
4. Exercise the splash-to-game transition, a scene reload, arrow-key bindings, and bulk auto-claim behavior, matching
   the original PR's runtime coverage.
5. If a crash recurs, capture the mod log and the full `instructionByteStream`. A repaired trampoline must not contain
   an `UDF` word followed by a relocation literal equal to a Capstone register ID.

## Optional diagnostic hardening

The current report is sufficient to diagnose this incident, so new diagnostics are not a prerequisite for the fix. A
future debug-only ARM64 trampoline registry would make ownership immediate for novel crashes. For each installed
detour it should record the target, wrapper, allocated trampoline range, copied-byte count, source location when
available, decoded instruction ID and operand type for every relocation, and a bounded hex dump. A crash PC could then
be mapped to the owning hook without requiring the exact GameAssembly binary.

## Confidence and limitations

- **Confirmed:** exact current-beta artifact identity and UUID; exact source SHA; absence of #203 from that SHA;
  generated SPUD trampoline allocation; corrupted `UDF #0x14`; `0xF7` register-ID literal; same relocation mechanism
  as #203.
- **Strong inference:** ownership by `il2cpp_unity_liveness_finalize` / `InstallObjectTrackers()`.
- **Unknown without runtime retest:** whether any separate post-#203 crash exists in a newly built artifact which
  actually contains the repair.

The strongest defensible conclusion is that this report validates the original #203 root cause against a correctly
installed but still pre-fix `dev` artifact.
