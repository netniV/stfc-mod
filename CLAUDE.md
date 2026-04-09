# STFC Community Mod — Claude Code Instructions

Read `agents.md` at the start of every session. It contains the full project overview, key file paths, deployment process, game file access policy, and the complete agent roster with `@agents/*.md` references. Load the relevant agent file(s) for the task at hand.

## Critical Rules

**Shell commands:**
- Issue one command at a time. Never chain commands with `&&`.
- Each Bash call must do one thing. Chained commands cannot be pre-authorized, cannot be individually rerun, and require separate authorization on every execution.

**Git:**
- Never commit directly to `main`.
- Never commit the `.claude/` directory.
- Always co-author AI-assisted commits: `Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>`
- Do not chain `cd` and `git` in a single command — use `git -C <path>` or separate calls.

**Build:**
- Always run `xmake` from the repo root.
- Never run `xmake build mods` — it produces a stale DLL.

**Game directory:**
- All agents have read-only access to the game directory by default.
- Write/execute access is restricted to the deploy and cleanup agents as defined in `agents.md`.
- Never write to the game directory without first confirming the game is not running.
