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
