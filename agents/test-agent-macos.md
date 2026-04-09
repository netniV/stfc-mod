---
name: test-agent-macos
description: Runs end-to-end tests on macOS by deploying the dylib, launching the game, and verifying JSON exports.
---

# Test Agent (macOS E2E)

## Role

Run end-to-end tests on macOS. macOS E2E testing is handled by contributors with macOS setups. This agent covers that workflow.

For unit tests that do not require the game, see @agents/test-agent.md.
For macOS builds, see @agents/build-agent-macos.md.

## Setup Notes

Before running E2E tests on macOS:
- Confirm the macOS game installation directory for your setup
- Confirm the dylib injection mechanism (the macOS equivalent of the `version.dll` proxy used on Windows)
- Adapt the test cycle from @agents/test-agent-windows.md for macOS paths and process management

## Boundaries

**Always:**
- Confirm the macOS game installation path before deploying or running tests — it varies by setup

**Never:**
- Assume the macOS game path matches the Windows path
