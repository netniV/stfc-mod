[CmdletBinding()]
param([string]$TomlInclude)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
function Get-PermissionState([string]$Path) {
    $acl = Get-Acl -LiteralPath $Path
    # Windows can normalize descriptor control bits; compare actual rules,
    # ownership and inheritance protection rather than serialized SDDL spelling.
    [ordered]@{
        Owner = $acl.Owner
        Group = $acl.Group
        Protected = $acl.AreAccessRulesProtected
        Rules = @($acl.Access | Select-Object IdentityReference, FileSystemRights,
            AccessControlType, IsInherited, InheritanceFlags, PropagationFlags)
    } | ConvertTo-Json -Depth 5 -Compress
}
Push-Location $repoRoot
try {
    if (-not $TomlInclude) {
        $packageRoot = Join-Path $env:LOCALAPPDATA '.xmake/packages/t/toml++'
        $header = Get-ChildItem -LiteralPath $packageRoot -Recurse -Filter toml.h |
            Where-Object { $_.Directory.Name -eq 'toml++' } | Select-Object -First 1
        if (-not $header) { throw 'Build with AX first, or supply -TomlInclude.' }
        $TomlInclude = $header.Directory.Parent.FullName
    }
    New-Item -ItemType Directory -Force build/config-save-test | Out-Null
    & clang++ --driver-mode=cl /std:c++latest /EHsc /MT /Imods/src "/I$TomlInclude" `
        tests/config_save_test.cc mods/src/config_save.cc /Febuild/config-save-test/test.exe `
        /Fobuild/config-save-test/ -Wno-deprecated-literal-operator
    if ($LASTEXITCODE -ne 0) { throw 'Config save test compilation failed.' }
    $fixtureRoot = Join-Path $repoRoot ('build/config-save-test/' + [guid]::NewGuid())
    & ./build/config-save-test/test.exe (Join-Path $fixtureRoot 'created')
    if ($LASTEXITCODE -ne 0) { throw 'Initial config creation regression failed.' }
    $fixtureRoot = Join-Path $fixtureRoot 'permissions'
    # Establish the baseline independently, before the first production save.
    New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
    $testFile = Join-Path $fixtureRoot 'settings.toml'
    Set-Content -LiteralPath $testFile -Value 'enabled = false'
    if (-not ((Get-Acl -LiteralPath $testFile).Access | Where-Object IsInherited)) {
        throw 'Fixture must have inherited permission entries.'
    }
    $inherited = Get-PermissionState $testFile
    & ./build/config-save-test/test.exe $fixtureRoot
    if ($LASTEXITCODE -ne 0 -or (Get-PermissionState $testFile) -ne $inherited) {
        throw 'Inherited ACL regression failed.'
    }
    $acl = Get-Acl -LiteralPath $testFile
    $acl.SetAccessRuleProtection($true, $true)
    Set-Acl -LiteralPath $testFile -AclObject $acl
    $explicit = Get-PermissionState $testFile
    & ./build/config-save-test/test.exe $fixtureRoot
    if ($LASTEXITCODE -ne 0 -or (Get-PermissionState $testFile) -ne $explicit) {
        throw 'Explicit ACL regression failed.'
    }
    & clang++ --driver-mode=cl /std:c++latest /EHsc /MT /Imods/src "/I$TomlInclude" `
        tests/config_save_failure_test.cc /Febuild/config-save-test/failure-test.exe `
        /Fobuild/config-save-test/ -Wno-deprecated-literal-operator
    if ($LASTEXITCODE -ne 0) { throw 'Config failure test compilation failed.' }
    & ./build/config-save-test/failure-test.exe (Join-Path $fixtureRoot 'failures')
    if ($LASTEXITCODE -ne 0) { throw 'Config failure regression failed.' }
    & clang++ --driver-mode=cl /std:c++latest /EHsc /MT /Imods/src "/I$TomlInclude" `
        tests/toml_editor_test.cc mods/src/toml_editor.cc mods/src/config_save.cc `
        /Febuild/config-save-test/editor-test.exe /Fobuild/config-save-test/ -Wno-deprecated-literal-operator
    if ($LASTEXITCODE -ne 0) { throw 'TOML editor test compilation failed.' }
    & ./build/config-save-test/editor-test.exe (Join-Path $fixtureRoot 'editor')
    if ($LASTEXITCODE -ne 0) { throw 'TOML editor regression failed.' }
    & clang++ --driver-mode=cl /std:c++latest /EHsc /MT /Imods/src "/I$TomlInclude" `
        tests/runtime_config_writer_test.cc /Febuild/config-save-test/worker-test.exe `
        /Fobuild/config-save-test/ -Wno-deprecated-literal-operator
    if ($LASTEXITCODE -ne 0) { throw 'Runtime writer test compilation failed.' }
    & ./build/config-save-test/worker-test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Runtime writer regression failed.' }
    & clang++ --driver-mode=cl /std:c++latest /EHsc /MT /Itests `
        tests/runtime_config_test.cc /Febuild/config-save-test/adapter-test.exe /Fobuild/config-save-test/
    if ($LASTEXITCODE -ne 0) { throw 'Native adapter test compilation failed.' }
    & ./build/config-save-test/adapter-test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native adapter regression failed.' }
    foreach ($mode in @('idle', 'deadline', 'finished', 'missing-handle')) {
        $outputPath = Join-Path $fixtureRoot ('force-' + $mode + '.txt')
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $child = Start-Process -FilePath (Join-Path $repoRoot 'build/config-save-test/adapter-test.exe') `
            -ArgumentList $mode -WindowStyle Hidden -PassThru -RedirectStandardOutput $outputPath
        if (-not $child.WaitForExit(5000)) {
            $child.Kill()
            throw "Force-close fixture stalled: $mode"
        }
        $child.Refresh()
        if ($child.ExitCode -ne 1) { throw "Force-close fixture did not terminate: $mode" }
        if ($mode -eq 'deadline') {
            if ($timer.ElapsedMilliseconds -lt 450 -or
                (Get-Content $outputPath -Raw) -notmatch 'pending cancellation requested') {
                throw 'Deadline did not allow best effort and request cancellation.'
            }
        }
    }
    Write-Output 'Native force-close child-process fixtures passed (500ms requested deadline; 5s harness watchdog).'
} finally {
    Pop-Location
}
