# Standalone policy tests: no game, IL2CPP, or mod DLL required.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    & clang++ -std=c++23 -Imods/src tests/action_queue.cc -o build/action_queue_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Action queue test compilation failed.' }
    & ./build/action_queue_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Action queue regression failed.' }
} finally { Pop-Location }
