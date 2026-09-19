# Run after the workspace's normal AX Windows releasedbg build.
# Uses production MapKey/ModifierKey objects from mods.lib; does not launch or modify the game.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try {
    if (-not (Test-Path -LiteralPath 'build/windows/x64/releasedbg/mods.lib')) {
        throw 'Run the normal Windows releasedbg build first.'
    }
    # Microsoft link.exe is required for the /GL objects in mods.lib (lld cannot read them).
    & clang++ --driver-mode=cl -fuse-ld=link /std:c++latest /EHsc /MT /Imods/src `
        tests/shortcut_hint_cache.cc /Fobuild/shortcut_hint_cache.obj /Febuild/shortcut_hint_cache.exe `
        /link /LTCG /LIBPATH:mods/src/il2cpp build/windows/x64/releasedbg/mods.lib
    if ($LASTEXITCODE -ne 0) { throw 'Shortcut hint test compilation failed.' }
    & ./build/shortcut_hint_cache.exe
    if ($LASTEXITCODE -ne 0) { throw 'Shortcut hint cache regression failed.' }
} finally {
    Pop-Location
}
