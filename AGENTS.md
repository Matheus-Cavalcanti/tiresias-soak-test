# AGENTS.md

## Project
Zephyr/NCS firmware for Tiresias DK.

## Agent Rules
- Never run builds, flash commands, or hardware tests. The developer does those manually.
- Keep changes scoped to the requested files/behavior.
- Prefer compile-time/devicetree configuration over runtime parsing when simple.
- Preserve existing Zephyr style and error handling patterns.
- Do not rewrite public APIs unless explicitly requested.
- Do not revert unrelated worktree changes.

## Useful Checks
Allowed:
- `git diff --check`
- `clang-format -i <edited files>`
- `rg`/`sed`/`git diff` for inspection

Not allowed unless explicitly requested:
- `west build`
- `cmake --build`
- `ninja`
- flashing/debugging commands