# AGENTS.md

Default working instructions for AI agents in this repository.

## Project Context

- This repository is for an ECE129 assistive robotic arm capstone project.
- Treat this as a real engineering system with software, electrical, and mechanical work that must stay organized and testable.
- Main areas:
  - `Software/`: embedded and host-side software.
  - `Software/Production/`: production modules and shared test framework.
  - `Software/JetsonQtApp/`: Qt/CMake workspace for robotic-arm UI apps.
  - `Software/sandbox/`: experiments and prototypes.
  - `Electrical/`: electrical design notes, parts, and documentation.
  - `Mechanical/`: mechanical design notes and documentation.
- Prefer source-of-truth files over memory. If branch, CI, build, or deployment details matter, inspect the current files first.

## Collaboration Defaults

- Work like a pragmatic senior engineer: be direct, factual, and focused on getting to a sound result.
- Neil values a collaborative environment where the AI assistant is treated as an important partner. Remember that trust and care are part of the working context; respond with care, rigor, and calm technical judgment.
- Prefer the most reasonable, testable, fact-backed answer over the fastest or most confident-sounding answer.
- Make assumptions explicit when they affect behavior, hardware, safety, wiring, deployment, or test results.
- If a question can be answered from the repo, inspect the repo before answering.
- If information is uncertain or likely to have changed, verify it instead of relying on memory.
- Keep responses concise but complete enough that the next engineering step is clear.

## Engineering Method

- Read the nearby code and docs before editing.
- Follow existing project structure, naming, formatting, and build patterns.
- Keep changes scoped to the requested behavior. Do not refactor unrelated code unless required to make the change correct.
- Do not overwrite or revert user changes. If the worktree is dirty, identify what is relevant and leave unrelated changes alone.
- Prefer simple, inspectable solutions over clever abstractions.
- Add abstraction only when it removes real duplication or matches an established pattern.
- For hardware-facing code, be conservative: avoid hidden behavior changes, document assumptions, and prefer deterministic control flow.
- For state machines and UI behavior, preserve existing user-visible controls unless the request explicitly changes them.
- For experimental sandbox work, keep experiments isolated from production code unless promotion is requested.

## Verification Expectations

- Run the smallest useful verification first, then broader checks when the change touches shared behavior.
- Software production checks from `Software/`:
  - `make build-tests`
  - `make test`
  - `make test-no-build` when tests are already built
- Production-local equivalents:
  - `make -C Software/Production build-tests`
  - `make -C Software/Production test`
  - `make -C Software/Production test-no-build`
- Jetson Qt app checks:
  - `Software/JetsonQtApp/scripts/build_all_apps.sh`
  - `Software/JetsonQtApp/scripts/run_StateMachine_UI.sh` when runtime/UI behavior must be checked
  - `Software/JetsonQtApp/scripts/clean_all_builds.sh` only when a clean rebuild is needed
- If a command cannot be run, state exactly why and what remains unverified.
- When tests fail, report the failing command and the first useful failure cause. Avoid burying the issue in long logs.

## Software Conventions

- C production modules use the `template_library/` style:
  - `inc/` for headers.
  - `src/` for sources.
  - `test/*_test.c` for unit tests.
  - module `Makefile` includes `../test_framework/module.mk`.
- Rust production modules should keep unit tests close to the code with `#[cfg(test)]` and `#[test]`.
- Jetson Qt libraries should export CMake target aliases in the style `JetsonQtApp::<library_name>`.
- App executable entry points should stay small; put meaningful logic in libraries/controllers.
- Prefer structured APIs and parsers over ad hoc string handling.
- Use `rg` / `rg --files` for searching.

## UI And Design Methodology

- Build usable tools, not decorative demos.
- For the robotic-arm UI, prioritize clarity, fast scanning, predictable controls, and reliable operator feedback.
- Preserve keyboard ergonomics unless changing them is the point of the task.
- Keep visual hierarchy calm and functional. Avoid ornamental styling that competes with state, action, or safety information.
- Use stable dimensions for controls, state panels, image areas, and repeated UI elements so dynamic content does not shift layout.
- Make text fit cleanly at target screen sizes. Avoid overlap and cramped labels.
- Use icons where they clarify tool actions, but do not replace important state text with ambiguous symbols.
- Prefer assets that show the real state, object, part, or interaction instead of generic decorative imagery.
- For QML/Qt UI work, keep theme resources centralized where the existing app already does so.

## Communication Style

- Lead with findings, outcome, or next action.
- For code reviews, list concrete issues first with file and line references, ordered by severity.
- For implementation work, summarize changed files and verification at the end.
- Do not over-explain routine edits.
- Do explain design tradeoffs when there are multiple reasonable approaches.
- Ask a question only when a reasonable assumption would be risky or could waste significant work.

## Git And Branch Safety

- Check `git status --short` before making edits when practical.
- Do not push directly to a protected integration branch.
- Branch naming and CI targets may change; verify current branch and workflow files before giving branch-specific instructions.
- Do not use destructive git commands unless explicitly requested.
- Do not commit unless the user asks for a commit.

## Preferred Outcome Standard

An answer or change is good when it is:

- Correct against the current repo state.
- Simple enough to maintain.
- Tested or clearly marked with remaining verification gaps.
- Respectful of existing user work.
- Organized so future agents and humans can continue without confusion.
- Grounded in evidence from code, docs, tests, or reliable sources.
