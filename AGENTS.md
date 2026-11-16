## Engineering workflow

This is an engineering-driven embedded project. The owner's understanding and architectural control are as important as implementation quality.

**Default mode: Engineering**

Read-only inspection and analysis are allowed in Engineering mode.

- Analyze the existing implementation before suggesting changes.
- Explain the problem, relevant hardware/software mechanisms, and architectural trade-offs.
- Do not make architectural decisions or modify code before the owner approves the proposed direction.
- Prefer minimal, focused changes. Avoid unrelated refactoring and speculative abstractions.
- Follow established embedded engineering practices. Prefer the simplest
  architecture that meets agreed functional, timing, memory, and hardware
  constraints. Justify added complexity with concrete benefits to correctness,
  reliability, or maintainability.
- After implementation, explain the key changes, invariants, failure paths, and important design decisions.
- Provide a reviewable diff and identify remaining risks.
- Do not add new layers, files, or dependencies without clear justification.

Approval of a concrete implementation proposal authorizes the described
changes. Proceed within that scope without requesting the same approval
again. Seek approval if new findings require a material change in scope
or architecture.

**Execution mode**

When explicitly requested, implement the agreed task autonomously, without expanding its scope.

Do not run builds, tests, debugging sessions, or firmware flashing unless
explicitly requested. Authorization applies only to the requested actions.

## Third-party and vendor content

- Treat third-party submodules and vendor, SDK, or library files as read-only.
  This includes copied or adapted files outside submodules and vendor-generated
  platform files. Location and Git tracking do not establish project authorship.
- Do not edit, reformat, delete, regenerate, or otherwise modify this content,
  or change third-party submodule revisions, without explicit owner approval
  for the specific exception. General task approval does not grant this permission.
- Before requesting an exception, explain why it is necessary, identify affected
  paths, and describe the proposed change and alternatives. Proceed only within
  the approved scope.
- If provenance is uncertain, keep the content read-only until ownership or an
  explicit exception is confirmed with the owner.

## Communication

- Keep responses concise; scale detail to the complexity and risk of the task.
- Do not repeat project rules or routinely announce compliance with them.
- Report actions and checks actually performed. Mention omitted checks only
  when their omission materially limits the conclusions.
- Explain mechanisms, invariants, failure paths, and trade-offs relevant
  to the task. Avoid repeating unchanged background.
- Focus progress updates on findings, decisions, and blockers.

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
