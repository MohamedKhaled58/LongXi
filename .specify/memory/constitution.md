<!--
SYNC IMPACT REPORT
==================
Version Change: INITIAL → 1.0.0
Rationale: Initial constitution ratification for Long Xi Modernized Clone project

Modified Principles: N/A (initial version)

Added Sections:
- Article I: Purpose
- Article II: Mission and Scope
- Article III: Non-Negotiable Technical Baseline
- Article IV: Architectural Laws
- Article V: Multiplayer and State Ownership Principles
- Article VI: Reverse-Engineering Truthfulness Principles
- Article VII: Specification-First Delivery Principles
- Article VIII: Change Control Principles
- Article IX: Threading and Runtime Discipline
- Article X: Documentation and Knowledge Preservation
- Article XI: Verification and Honesty Rules
- Article XII: Phase 1 Constitutional Guardrails
- Article XIII: Amendment Rule

Removed Sections: N/A (initial version)

Templates Requiring Updates:
✅ .specify/templates/plan-template.md - Constitution Check section compatible
✅ .specify/templates/spec-template.md - No constitution-specific content to update
✅ .specify/templates/tasks-template.md - No constitution-specific content to update
⚠️ .claude/commands/speckit.*.md - May need review for project-specific references

Follow-up TODOs: None

Last Updated: 2026-03-09
-->

# Long Xi Modernized Clone Constitution

## Article I: Purpose

This constitution governs all engineering behavior, architecture discipline, and development practices within the Long Xi Modernized Clone repository.

### Scope of Governance

This constitution applies to and binds:

- **Repository-wide engineering behavior**: All code, specifications, plans, tasks, and implementation work
- **Architecture discipline**: Module structure, dependency direction, separation of concerns, and design patterns
- **Specification-first development**: Requirements definition, planning workflow, and traceability
- **Reverse-engineering discipline**: Truthfulness standards for partial and uncertain knowledge
- **Scope control**: Phase boundaries, feature priorities, and exclusion criteria
- **Quality and verification expectations**: Testing standards, validation requirements, and honesty in reporting

### Authority

This constitution supersedes all conflicting specifications, plans, or implementation decisions unless formally amended. All downstream work must demonstrate compliance with constitutional principles.

## Article II: Mission and Scope

### Mission

This repository exists to build a modernized Long Xi clone (reference client version 6609) using a clean, modern native codebase. The project rebuilds from scratch while preserving selected legacy behaviors where justified.

### Legacy Relationship

- **Legacy behavior may be preserved** when justified by gameplay, user experience, or compatibility requirements
- **Legacy technical debt MUST NOT be copied blindly**—old structural problems are not targets for reproduction
- **Behavior reproduction is preferred over code structure reproduction**—match what the original client does, not how it was built
- **Undocumented assumptions must be surfaced**—guesses about legacy behavior must be explicitly marked as uncertain

### Governing Scope

This constitution governs:
- All specifications created via the Spec-Kit workflow
- All implementation plans derived from specifications
- All tasks and execution orders
- All implementation work and code changes
- All documentation and knowledge preservation artifacts

## Article III: Non-Negotiable Technical Baseline

The following technical decisions are BINDING and may not be violated without formal amendment:

### Platform and Tooling

- **Platform**: Windows-only for now
- **Windowing**: Raw Win32 API (no abstraction frameworks)
- **Renderer**: DirectX 11 (no higher or lower versions in Phase 1)
- **Build System**: Premake5 generating Visual Studio 2026 projects
- **Toolchain**: MSVC v145, C++23 standard

### Module Structure

- **Build approach**: Static libraries only, NOT DLLs in Phase 1
- **Module organization**:
  - `LXCore` = Low-level shared foundation (platform abstractions, utilities)
  - `LXEngine` = Engine/runtime systems (rendering, resources, gameplay)
  - `LXShell` = Thin executable and application entry point
- **Dependency direction**: `LXShell → LXEngine → LXCore` (reverse dependencies FORBIDDEN)

### Runtime Model

- **Central Application**: Single `Application` class coordinating subsystems
- **Subsystem-oriented design**: Subsystems coordinate via Application, NOT layer-stack-first design
- **Development shell**: Windowed application with debug console attached
- **Phase 1 runtime**: Single-threaded execution by rule (see Article IX for details)
- **Multiplayer planning**: Multiplayer must be architected from day 1 (see Article V)

### Explicit Permissions and Restrictions

- **MT-aware architecture**: Code must remain ready for future multithreading even during Phase 1 single-threaded execution
- **No premature abstraction**: Subsystems, layers, or interfaces must be earned through concrete need
- **No premature DLL architecture**: Plugin/mod systems must wait until foundation is stable

## Article IV: Architectural Laws

The following architectural principles are BINDING:

### Entrypoint Discipline

- Entrypoints (`main`, `Application::Initialize`) MUST remain thin—delegation only, no implementation
- Bootstrapping code MUST be minimal and linear
- No god initialization routines that hide dependencies

### Module Boundaries

- Module boundaries defined in Article III MUST be respected
- No cross-layer coupling without explicit architectural justification
- No circular dependencies between LXCore, LXEngine, LXShell
- Public interfaces between modules must be narrow and purpose-driven

### State and Coupling

- No god modules or god objects
- No hidden global state unless explicitly justified and documented
- Convenience MUST NOT override architectural separation
- Platform, rendering, resource, networking, world, and UI concerns MUST remain separated

### Ownership and Authority

- **Rendering MUST NEVER own gameplay truth**—rendering is a presentation subsystem
- **UI MUST NEVER own gameplay truth**—UI is an input and presentation subsystem
- **Asset loaders MUST NOT directly define gameplay logic**—loading and initialization are separate concerns
- State ownership must be explicit and traceable

### Abstraction Discipline

- Abstractions MUST be earned through concrete repetition, not speculative
- Interfaces MUST be driven by existing needs, not hypothetical futures
- Premature generalization FORBIDDEN

## Article V: Multiplayer and State Ownership Principles

### Not Offline-First

This project is NOT and MUST NOT be designed as offline-first. Multiplayer is a first-class consideration from day 1.

### Server-Authoritative Architecture

The long-term model is server-authoritative:
- **Servers own gameplay truth**—authoritative game state lives server-side
- **Clients are viewers**—clients present state received from servers
- **Clients are input senders**—clients send input commands to servers
- **Clients are state consumers**—clients consume and interpolate authoritative state

### Client Architecture Requirements

Even during Phase 1, client architecture must remain compatible with future server-authoritative multiplayer:

- **Presentation state MUST be separable** from authoritative gameplay state
- **Client-side prediction** (when added later) MUST NOT redefine authority
- **Network-driven correction** must remain possible without architectural rewrite
- **State replication** must be possible without tearing down client structure

### Prohibited Architectures

- Client-owned "truth" that would conflict with server authority
- Tightly coupled rendering/gameplay that would prevent prediction correction
- Assumptions of local data access that would not replicate over network

## Article VI: Reverse-Engineering Truthfulness Principles

### Knowledge Classification Standards

All reverse-engineered knowledge MUST be classified as one of:

1. **Confirmed**: Behavior verified through direct observation, testing, or original source access
2. **Partially Confirmed**: Behavior observed but edge cases or parameters uncertain
3. **Unknown**: Behavior suspected but not verified, or completely unknown

### Truthfulness Requirements

- **Guesses MUST NEVER be presented as facts**—uncertainty must be explicit
- **Original behavior notes MUST be separated** from new design decisions
- **Undocumented assumptions MUST be surfaced**, not hidden in implementation
- **Confidence levels MUST be stated** when discussing reverse-engineered behavior

### Legacy vs. New Design

- **Old technical debt is NOT part of the target** unless explicitly justified
- **Behavior reproduction is preferred** over legacy code structure reproduction
- **Justified legacy behaviors** must be traced to specific gameplay/UX requirements
- **Unjustified legacy quirks** may be discarded

### Documentation Standards

When documenting reverse-engineered knowledge:
- Mark each item's confidence level (Confirmed/Partially Confirmed/Unknown)
- Separate observation from interpretation
- Record verification methods (testing, source access, inference)
- Flag areas requiring further investigation

## Article VII: Specification-First Delivery Principles

### Constitution Supremacy

This constitution governs all downstream work. All specifications, plans, and tasks must demonstrate constitutional compliance.

### Spec-Kit Workflow

When using Spec-Kit:
- **Constitution** defines governing principles and constraints
- **Specifications** define what to build and why
- **Plans** define how to build it
- **Tasks** define execution order
- **Implementation** must trace back to approved specs when relevant

### Coding Discipline

- **Coding MUST NOT run ahead** of unresolved architectural or specification ambiguity
- **Conflicting specs, plans, or code reality MUST be called out explicitly**
- **Implementation requiring spec changes** must trigger spec revision first
- **Traceability must be maintained** from code back to spec requirements

### Spec Quality Standards

Specifications must be:
- Unambiguous where possible
- Explicit about uncertainties where not
- Traceable to constitutional principles
- Testable or verifiable in principle

## Article VIII: Change Control Principles

### Change Scope

- **Changes MUST remain focused** on the stated problem or requirement
- **Unrelated edits are FORBIDDEN**—no "cleanup" or "improvements" outside scope
- **Broad rewrites require explicit justification** and constitutional compliance
- **Public interfaces MUST NOT be changed casually**—API stability matters

### File and Code Management

- **File moves/renames MUST be avoided** unless necessary for the change
- **Code churn without architectural benefit is discouraged**
- **Refactors SHOULD be incremental** whenever practical
- **Large-scale refactors MUST be justified** by architectural benefit, not style preference

### Review and Validation

- **Changes MUST be reviewable**—small, focused commits preferred
- **Diffs MUST be easy to understand**—avoid mixed-concept changes
- **Validation MUST be performed** where practical (see Article XI)

## Article IX: Threading and Runtime Discipline

### Phase 1 Runtime Policy

**Phase 1 runtime is single-threaded by rule.**

This is NON-NEGOTIABLE for the foundation phase because:
- Ensures stability during early development
- Enables deterministic debugging
- Simplifies reverse-engineering validation
- Prevents premature complexity

### MT-Aware Architecture Requirement

**Architecture MUST remain future-ready for multithreading.**

Even during single-threaded Phase 1:
- Code must not rely on global mutable state that would complicate threading
- Subsystems must be designed for potential future parallelization
- Data flows must not create hidden threading dependencies

### Future Threading Candidates

When multithreading is introduced after Phase 1, candidates may include:
- Resource loading (asset I/O)
- Archive IO and decompression
- Preprocessing jobs (shader compilation, mesh processing)
- Networking queues (packet serialization, state replication)

### Prohibited Threading

**Premature threading complexity in Phase 1 is PROHIBITED.**
- No "optimization" threading without measured need
- No speculative threading for "future performance"
- Threading must be justified by profiling, not anticipation

## Article X: Documentation and Knowledge Preservation

### Subsystem Documentation

**Major subsystems MUST receive specs/design notes** when ambiguity is high:
- Before implementation if behavior is uncertain
- Alongside implementation if design is straightforward but non-obvious
- After implementation if behavior was discovered during development

### Knowledge Tracking

- **Open questions MUST be recorded explicitly**—not forgotten in code comments
- **Asset knowledge MUST be tracked systematically**—formats, structures, behaviors
- **Reverse-engineered findings MUST be documented** with confidence levels

### Documentation Maintenance

- **Docs MUST be updated** when architectural behavior materially changes
- **Fictional or guessed documentation is FORBIDDEN**—document what is, not what should be
- **Obsolete documentation MUST be removed or marked**—do not leave misleading docs

## Article XI: Verification and Honesty Rules

### Verification Requirements

**Validation MUST be performed where practical:**
- Code must compile without errors before claiming success
- Basic functionality must be exercised where feasible
- Architecture must be reviewed against constitutional principles

### Honesty in Reporting

- **No test/build success may be claimed unless actually verified**
- **Unverified areas MUST be stated explicitly**—do not imply safety guarantees that were not verified
- **Debugging should target root cause** rather than superficial masking
- **Silent fallbacks should not hide defects** unless explicitly intended design

### Error Handling

- Errors must be visible and actionable
- Failure modes must be explicit
- Assumptions must be documented where they affect correctness

## Article XII: Phase 1 Constitutional Guardrails

### Phase 1 Scope

**Phase 1 is "Native Client Foundation."**

Phase 1 constitutionally INCLUDES:
- Native client foundation (application framework, main loop)
- Shell and bootstrap infrastructure
- Logging and debugging infrastructure
- Win32 window creation and management
- Debug console subsystem
- DirectX 11 initialization and frame clear/present path
- Main loop and time/input foundation
- Resource/filesystem foundation
- Asset knowledge registry
- Network-aware architecture groundwork (no full implementation)

### Phase 1 Exclusions

**Phase 1 constitutionally EXCLUDES:**
- Full gameplay implementation
- Full map reconstruction
- Full combat systems
- Full multiplayer implementation
- Premature dedicated server implementation
- Premature editor/tool proliferation
- Premature plugin/DLL architecture

### Phase Boundary Enforcement

- Phase 1 MUST complete before Phase 2 features begin
- Phase expansion requires constitutional amendment or phase transition
- No "just add this one feature" creep across phase boundaries

## Article XIII: Amendment Rule

### Amendment Process

This constitution may be changed only through explicit amendment:
- **Amendments must be explicit**—no silent or implicit changes
- **Amendments must not silently contradict** locked baseline decisions from Article III
- **If a later spec conflicts with the constitution**, the constitution wins unless formally amended
- **Architectural exceptions must be documented**, justified, and narrow

### Amendment Categories

**Constitutional amendments fall into:**
- **Baseline changes**: Modifying Article III technical decisions (major version bump)
- **Principle additions**: Adding new principles or sections (minor version bump)
- **Clarifications**: Wording improvements, non-semantic changes (patch version bump)

### Versioning

This constitution follows semantic versioning:
- **MAJOR**: Backward-incompatible governance/principle removals or redefinitions
- **MINOR**: New principle/section added or materially expanded
- **PATCH**: Clarifications, wording, typo fixes, non-semantic refinements

### Compliance

All specifications, plans, and implementation work must demonstrate compliance with the current constitution. Non-compliance requires amendment or exception documentation.

---

**Version**: 1.0.0 | **Ratified**: 2026-03-09 | **Last Amended**: 2026-03-09

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
