$sourceDirs = @("mods/src", "win-proxy-dll/src", "macos-dylib/src", "macos-loader/src")

foreach ($dir in $sourceDirs) {
    if (Test-Path $dir) {
        Get-ChildItem -Path $dir -Recurse -Include "*.cc", "*.h" |
            ForEach-Object { clang-format -i $_.FullName }
    }
}
