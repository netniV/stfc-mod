# Startup config saves

`Config::Save` writes complete TOML documents for two startup callers: the initial
default config and the generated runtime snapshot. It keeps `File::MakePath`
routing and the existing generated-file warning. Save errors are logged once by
the caller; startup continues with the in-memory configuration.

`SaveConfigDocument` serializes with toml++, parses the output before touching
disk, exclusively creates a sibling temporary file, checks writing and closing,
and replaces the destination. No threads, frame callbacks, runtime controls or
shutdown interception are installed. This is not the preserving TOML editor:
whole-document saves do not merge concurrent setting changes or preserve comments.

Windows uses `ReplaceFileW` to preserve existing permissions and streams, with a
temporary backup for its documented partial-failure cases. A missing destination
falls back to a non-replacing move. The caller's startup existence check is not an
exclusive create transaction. Ordinary failures clean up the temporary file; partial
replacement failures retain recovery files and report their location. The backup
name is the reported temporary path plus `.bak`. Recovery is not automatic.
macOS uses rename after copying the existing permission bits. Extended metadata
and hard-link identity are not preserved by that path. Existing symlinks are
resolved before staging. Replacement requires directory permissions in addition
to any file access checks; it cannot exactly match an in-place overwrite.

Successful close/replacement is not a guarantee against power loss. A forced exit
can leave a temporary file. No automatic stale-file sweep is installed.

Run the isolated Windows fixtures with `tests/run-config-save.ps1` after the
normal AX build has installed toml++; `-TomlInclude` can select another include
directory. Fixtures never access the installed game's files.

On macOS, run `bash tests/run-config-save.sh TOML_INCLUDE_DIR`. Both native macOS
CI jobs run these fixtures after the normal build, including permission-bit and
symlink checks. The failure fixture injects short writes and failed closes at
compile time; it does not install test controls in the mod.

Native behavior references:
- [Windows ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)
- [POSIX rename](https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html)
