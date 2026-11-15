---
name: c-code-style
description: Write, edit, or review first-party C source and headers in nexnclxck-one-fw using the project's pinned C style fork.
---

# Project C code style

Follow [project instructions](../../../AGENTS.md), including approval,
scope, toolchain, and validation requirements.

Read the [canonical skill](../../../tools/c-code-style/.claude/skills/c-code-style/SKILL.md)
and [coding rules](../../../tools/c-code-style/CODING_RULES.md).
Resolve references inside the canonical skill relative to its original location.

If the submodule is missing, initialize its pinned revision:
`git submodule update --init -- tools/c-code-style`.

Run commands from the firmware root, using only files within the task's scope.
Replace the canonical skill's Bash commands with native commands:

- Format edited files: `clang-format -i path/to/file.c`
- Verify formatting, only when requested:
  `clang-format --dry-run --Werror path/to/file.c`
