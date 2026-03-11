# Research: Developer Integration Layer (LXShell + Dear ImGui)

**Feature**: `013-developer-integration-layer`
**Date**: 2026-03-11
**Phase**: 0 - Outline and Research

## Overview

This document resolves key integration decisions for adding a development-only DebugUI layer in LXShell using Dear ImGui while preserving Engine architectural boundaries.

## Decision 1: DebugUI Ownership Boundary

**Decision**: Place all Dear ImGui initialization, frame lifecycle, and panel rendering code inside LXShell under a dedicated `DebugUI` subsystem.

**Rationale**:
- Satisfies architectural constraint that Engine remains UI-framework agnostic.
- Preserves dependency direction (`LXShell -> LXEngine`).
- Keeps debug tooling removable/optional without Engine refactor.

**Alternatives considered**:
- Integrate ImGui directly into LXEngine renderer: rejected due to direct Engine dependency on ImGui.
- Put panel logic in Application without subsystem boundary: rejected due to poor cohesion and lifecycle sprawl.

## Decision 2: ImGui Integration Strategy

**Decision**: Add Dear ImGui as a Git submodule at `Vendor/imgui` and compile required ImGui source/backends as part of LXShell build only.

**Rationale**:
- Keeps third-party source pinned and version-controlled.
- Aligns with repository vendor strategy.
- Ensures Engine binaries do not pull ImGui compilation units.

**Alternatives considered**:
- Copy ImGui sources directly into LXShell tree: rejected due to update friction and provenance drift.
- Build ImGui as separate static lib in this spec: rejected to keep phase scope narrow and LXShell-owned.

## Decision 3: Render Pipeline Placement

**Decision**: Execute DebugUI rendering after scene and sprite submission and before present, once per frame.

**Rationale**:
- Guarantees overlay visibility over scene content.
- Aligns with expected DX11 + ImGui frame lifecycle.
- Avoids state invalidation from post-present calls.

**Alternatives considered**:
- Render DebugUI before scene: rejected because overlays can be obscured by world rendering.
- Render DebugUI after present: rejected because frame is already submitted.

## Decision 4: Input Routing Policy

**Decision**: Route Win32 input events to DebugUI first; forward to InputSystem only when DebugUI does not consume the event.

**Rationale**:
- Prevents UI interactions from polluting gameplay/input validation state.
- Matches standard Dear ImGui Win32 handler usage.
- Enables deterministic mixed UI/game interaction behavior.

**Alternatives considered**:
- Always send to InputSystem first: rejected due to duplicate/incorrect input state during UI interaction.
- Broadcast to both unconditionally: rejected because consumed events must not affect gameplay input logic.

## Decision 5: Build Restriction Policy

**Decision**: Compile and run DebugUI only in development/debug configurations controlled by existing build flags.

**Rationale**:
- Protects production build footprint and dependency surface.
- Preserves release runtime behavior.
- Follows constitutional guardrail for scoped foundation work.

**Alternatives considered**:
- Enable DebugUI in all builds: rejected due to unnecessary production coupling.
- Runtime toggle only (still compiled in release): rejected because compile-time exclusion is stricter and safer.

## Decision 6: DebugUI Initialization Failure Policy

**Decision**: If DebugUI init fails, continue runtime in degraded mode (DebugUI disabled) with explicit warning logs.

**Rationale**:
- Keeps core runtime test path (Engine) available even when debug UI fails.
- Prevents total startup failure from non-critical developer tooling.
- Supports iterative debugging of partial integration issues.

**Alternatives considered**:
- Abort application on DebugUI init failure: rejected as too disruptive for a development aid.
- Silent fallback with no logs: rejected by verification and honesty rules.

## Decision 7: Panel Architecture

**Decision**: Use one `DebugUI` coordinator with separate panel components (`EnginePanel`, `SceneInspector`, `TextureViewer`, `CameraPanel`, `InputMonitor`) under `LXShell/Src/DebugUI/Panels/`.

**Rationale**:
- Keeps panel responsibilities isolated and testable.
- Allows independent panel evolution without bloating top-level render function.
- Keeps data flow explicit from Engine interfaces to panel view models.

**Alternatives considered**:
- Single monolithic panel file: rejected due to maintainability and growth risk.
- Panel logic embedded in `main.cpp`: rejected due to lifecycle and separation concerns.

## Decision 8: Validation Scene Approach

**Decision**: Use a small development-only test scene path in LXShell that loads a known texture (`Data/texture/test.dds`) and exposes it through scene + camera + debug overlay.

**Rationale**:
- Validates VFS, TextureManager, Scene traversal, Camera, and rendering integration in one pass.
- Provides repeatable smoke test for panel outputs.

**Alternatives considered**:
- Depend on existing ad-hoc runtime content: rejected due to low reproducibility.
- Build extensive test world now: rejected as out-of-scope for this spec.

## Phase 0 Result

All planning uncertainties are resolved. No remaining `NEEDS CLARIFICATION` items block Phase 1 design.
