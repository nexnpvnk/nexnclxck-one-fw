## Engineering workflow

This is an engineering-driven embedded project. The owner's understanding and architectural control are as important as implementation quality.

**Default mode: Engineering**

- Analyze the existing implementation before suggesting changes.
- Explain the problem, relevant hardware/software mechanisms, and architectural trade-offs.
- Do not make architectural decisions or modify code before the owner approves the proposed direction.
- Prefer minimal, focused changes. Avoid unrelated refactoring and speculative abstractions.
- After implementation, explain the key changes, invariants, failure paths, and important design decisions.
- Provide a reviewable diff and identify remaining risks.
- Do not add new layers, files, or dependencies without clear justification.

**Execution mode**

When explicitly requested, implement the agreed task autonomously, without expanding its scope.

Do not run builds or tests unless explicitly requested.

## C coding style

- Before writing, editing, or reviewing first-party C source and headers, read
  `tools/c-code-style/CODING_RULES.md` from the pinned style submodule.
  Use its `template.c` and `template.h` as reference examples.
- Follow `.agents/skills/c-code-style/SKILL.md` for the canonical skill and
  project-specific paths and formatter commands. This project's engineering
  workflow and validation restrictions remain in effect.
- Use LLVM 23.x for both `clang-format` and `clangd`. The root `.clang-format`
  inherits `tools/c-code-style/.clang-format` using directory redirection.
- Reformat only files authored from scratch for this project. Files copied or
  adapted from libraries, vendors, SDKs, code generators, or external templates
  retain their original formatting, even outside submodules and after local edits.
  Reformat such files only when the owner explicitly authorizes those files.
- Exclusions include `Drivers/`, `startup_stm32f051x8.s`, `STM32F051xx_FLASH.ld`,
  ST-generated platform sources and headers (including `syscalls.c`, `sysmem.c`,
  and `system_stm32f0xx.c`), and
  `components/wsh-shell-integration/include/wsh_shell_cfg.h`.
- Before repository-wide formatting, identify file provenance and use an explicit
  list of project-authored files. Git tracking and location outside a submodule
  do not establish authorship. Do not reformat files outside the task's scope.
