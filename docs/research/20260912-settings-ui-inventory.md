# Settings UI inventory: build261

This is a bounded metadata/code study, not a list of controls safe to expose
automatically. Native controls need a verified owner, applicability, readback and
write behavior. A key name or property alone does not establish any of those.

Source: local Windows x64 `dump.cs`, SHA256
`4f7591b4469ae596e5b8b3606e23ba0292043fa5b6c51c0cabcaa94c043d60f6`;
GameAssembly SHA256
`487af4bb9c697c353be9714359a97dddcece5dab872622a6c498a27bbfc44f40`.
Method names below are search anchors in that dump; they are not decompiled bodies.

## What resembles a settings manifest

There are at least two distinct mechanisms:

- `PersistentPrefsManager` owns `_savedData` and a cloud-loaded flag.
  `PersistentPrefsData.Preferences` is a repeated list of stored preference entries.
  It can inventory present entries, but absent/default keys need not appear.
  `GetBool/GetInt/GetFloat/GetString` accept `shouldAddKey`, defaulting to true;
  a diagnostic must use false or inspect existing entries to avoid creating keys.
- `GameSettingsManager.UserSettingsRegistry` exposes `IUserSettingsRegistry`.
  Its interface provides initialization, registration and typed lookups, without
  an enumeration method. Registered `IUserSetting` objects expose initialized and
  overridden state, defaults and typed values. This is not the cloud preference
  list. Several graphics settings derive from `NonSerializableUserSetting<T>`.

A protobuf descriptor describes serialization, not allowed player controls,
value ranges, or a complete setting manifest. No complete UI-safe manifest was
established by this study.

## Confirmation controls

| Candidate | Evidence and disposition |
| --- | --- |
| Fleet Commander ability use | `options/hide_fcaa_use_confirmation`; existing mod control already validated for readback and restart persistence. |
| Cutting Beam | `options/cutting_beam_confirmation`; native director has reader/toggle and player screenshot has its row. Keep native control. |
| Gravitational Blast | `options/chain_shot_confirmation`; director reader/toggle. Player identified GS-31 ability and verified native/mirror readback earlier. Keep native control. |
| Seek and Destroy | `options/auto_hunting_confirmation`; director reader/toggle. Player identified Dauntless ability and its General setting, with mirror readback verified. Keep native control. |
| Smart Speed-up | `auto_speedup_confirm_menu_popup_key`; director `IsOneClickSpeedupConfirmationOn` / toggle, visible in player screenshot. |
| Convert all artifact shards | `artifacts_batch_convert_confirmation_key`; director `IsArtifactsBatchConvertConfirmationOn` / toggle, visible in screenshot. |
| Remove armada participant | `armada/blockConfirm`; director `IsArmadaBlockConfirmationOn` / toggle, visible in screenshot. |
| Latinum / multiple-resource spend | Director readers/toggles and `options/hard_currency_spend_confirmation`, `options/multiple_resources_spend_confirmation`; both visible in screenshot. |
| Forbidden Tech upgrades | Existing mod-owned `ui.auto_confirm_ft_upgrade` bypass. New UI reads its inverse; this does not establish a cloud preference for FT. Runtime control validation is pending. |

The searched confirmation constants do not establish another missing confirmation
control beyond FC. Dialog helpers accept arbitrary `enabledPrefKey` strings, so
this is not an exhaustive proof that no other confirmation exists.

## Other candidates and exclusions

| Area | Evidence | Next step |
| --- | --- | --- |
| Fleet label detail/threshold | Existing mod Config profiles and zoom adapter; now two sections on one choice/slider page. | Validate both profiles, actual label changes, independent saves and restart. |
| Mod hotkeys | Existing parser, `MapKey` registrations; native `keybindings` is a separate owner. | Separate branch; keep mod capture/conflict rules distinct from game bindings. |
| Ship textures in system view | `options/improve_system_view_ship_texture_setting` and GameSettingsManager property, plus SettingsSectionDirector reader and change handler. | Native UI integration already has an apparent path; check Graphics before assuming it is missing. |
| Extended zoom, skip toasts, skip animations, mission dialogue on login | Existing manager flags and director reader/toggle pairs. | Inspect native General rows and applicability queries; do not duplicate blindly. |
| Graphics sub-options | `VSyncCountUserSetting`, star flares, corona plate, lens flare, background blur/camera, tessellation and texture-size setting types exist. Many are nonserializable. | Trace quality-preset initialization and runtime callers before proposing individual controls. They may be preset components rather than independent preferences. |
| On-screen diagnostic log | Native constants include `onscreen_errors_enabled`, queue size, message length and duration. | Debug-build research candidate only; establish live owner and bounded behavior before a UI proposal. |

## Next bounded probe

Follow the game-error probe runbook: opt-in debug capture, exact-client native
extent checks, original invoked once, and no continuous frame logging. Prefer the
already owned cloud-load seam after readiness is established. Inventory existing
preference keys/types/count once; capture values only for a small allowlist of
candidate settings. Record the owner and relevant caller/callee observations.
Do not dump account identifiers, tokens, arbitrary string/list contents, or call
getters that create missing preferences. An on-load observation is evidence of
availability, not authorization to expose a setter.

For the user, the useful next study check is the native Graphics/General list:
whether these candidate rows are already present and under which labels. Perform
that separately from the Fleet Labels slider smoke test to keep the evidence clear.
