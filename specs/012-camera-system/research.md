# Research: Camera System

**Feature**: `012-camera-system`  
**Date**: 2026-03-11  
**Phase**: 0 - Outline & Research

## Overview

This document records implementation decisions for introducing a Scene-owned camera that provides view/projection matrices to the DX11 renderer while preserving existing architectural constraints.

## Decision 1: Matrix Convention and Coordinate System

**Decision**: Use left-handed camera math with row-major `Matrix4` storage and Euler rotation order `Yaw(Y) -> Pitch(X) -> Roll(Z)` in degrees.

**Rationale**:
- Spec requirements explicitly require DirectX-compatible left-handed conventions.
- Existing rendering code already assumes row-major matrix handling patterns.
- SceneNode rotation convention already uses degree-based Euler vectors.

**Alternatives considered**:
- Right-handed matrices: rejected due to mismatch with DX11 conventions and spec.
- Column-major matrix storage: rejected due to inconsistency with current engine assumptions.
- Quaternion-only rotation: rejected as out of scope for Spec 012.

## Decision 2: Camera Matrix Recompute Strategy

**Decision**: Adopt dirty-flag recomputation. Setters mark matrices dirty; Scene performs one pre-render sync pass and recomputes only dirty matrices.

**Rationale**:
- Prevents hidden work in getters and avoids repeated recompute during frequent setter calls.
- Guarantees correctness at render boundary with deterministic timing.
- Keeps per-frame overhead low and predictable.

**Alternatives considered**:
- Recompute in each setter: rejected due to potential redundant work.
- Recompute in getters: rejected due to hidden side effects in const accessors.
- Recompute every frame unconditionally: rejected as unnecessary overhead.

## Decision 3: Initial Aspect Ratio Source

**Decision**: During `Scene::Initialize()`, derive camera aspect ratio from the current renderer/backbuffer dimensions.

**Rationale**:
- Ensures first rendered frame uses correct projection.
- Avoids temporary distortion until first resize event.
- Aligns with clarification outcome in spec.

**Alternatives considered**:
- Fixed 16:9 bootstrap aspect ratio: rejected due to first-frame mismatch risk.
- Manual caller-provided aspect ratio only: rejected due to increased coupling and setup fragility.

## Decision 4: Invalid Parameter Handling Policy

**Decision**: Clamp invalid camera parameters to safe ranges and emit warning logs.

**Rationale**:
- Keeps runtime stable under bad input while preserving observability.
- Matches clarified spec behavior.
- Avoids camera entering invalid matrix states.

**Alternatives considered**:
- Reject invalid input without applying change: rejected due to higher risk of stale/unexpected camera state.
- Error-level logging for recoverable input: rejected as too severe/noisy for expected runtime misuse.

## Decision 5: Renderer Interface Contract

**Decision**: Introduce consume-only `DX11Renderer::SetViewProjection(const Matrix4&, const Matrix4&)`. Do not expose renderer camera-matrix getters.

**Rationale**:
- Maintains ownership clarity: Scene/Camera own camera state.
- Prevents render-node coupling to renderer internal state cache.
- Supports future renderer internals refactoring without breaking scene API.

**Alternatives considered**:
- Renderer getters for view/projection: rejected per clarification and ownership constraints.
- Scene direct draw calls: rejected by scene architecture rules.

## Decision 6: Resize Propagation and Zero-Dimension Handling

**Decision**: Keep resize chain `Engine -> Scene -> Camera`. Ignore zero/negative dimensions for projection recompute; retain last valid projection.

**Rationale**:
- Matches current window-minimize behavior patterns in engine.
- Avoids invalid aspect ratio division and NaN matrix propagation.

**Alternatives considered**:
- Force fallback aspect ratio on zero size: rejected due to unnecessary matrix churn and ambiguity.
- Hard fail on zero size: rejected because minimize/restore is expected runtime behavior.

## Phase 0 Result

All technical context decisions are resolved. No remaining `NEEDS CLARIFICATION` items block Phase 1 design.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
