# Feature Specification: Developer Integration Layer (LXShell + Dear ImGui)

**Feature Branch**: `013-developer-integration-layer`  
**Created**: 2026-03-11  
**Status**: Draft  
**Input**: User description: "Spec 013 - Developer Integration Layer (LXShell + Dear ImGui)"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Initialize and Run Debug UI Lifecycle (Priority: P1)

As an engine developer, I want a DebugUI subsystem in LXShell to initialize after Engine startup and run every frame so I can inspect runtime state without modifying engine internals.

**Why this priority**: Without a reliable lifecycle, no debug tooling can be used. This is the foundation for all other inspection and validation workflows.

**Independent Test**: Launch LXShell in a development build and verify that DebugUI initializes successfully, renders each frame, and shuts down cleanly before renderer destruction.

**Acceptance Scenarios**:

1. **Given** LXShell starts in a development build, **When** Engine initialization succeeds, **Then** DebugUI initializes successfully using the active window and renderer context.
2. **Given** a running frame loop, **When** frame rendering occurs, **Then** DebugUI frame lifecycle executes once per frame.
3. **Given** application shutdown starts, **When** subsystem teardown runs, **Then** DebugUI shuts down before renderer teardown and no debug UI resources remain active.

---

### User Story 2 - Route Input Correctly Between Debug UI and Engine Input (Priority: P1)

As an engine developer, I want window input events to be offered to DebugUI first and only forwarded to InputSystem when not consumed so that editor interactions do not interfere with gameplay/input validation.

**Why this priority**: Incorrect input routing causes false validation results and unusable debug interactions.

**Independent Test**: Interact with DebugUI widgets while monitoring input state; verify consumed events do not appear in InputSystem while non-consumed events do.

**Acceptance Scenarios**:

1. **Given** a DebugUI widget has focus, **When** keyboard or mouse input occurs, **Then** the event is consumed by DebugUI and InputSystem does not process it.
2. **Given** DebugUI does not consume an event, **When** the event is received, **Then** InputSystem processes it normally.
3. **Given** mixed UI/game interactions in the same session, **When** events alternate between consumed and non-consumed, **Then** routing remains deterministic and consistent.

---

### User Story 3 - Inspect Runtime State Through Debug Panels (Priority: P1)

As an engine developer, I want dedicated panels for engine metrics, scene hierarchy, textures, camera parameters, and input state so I can validate subsystem behavior in real time.

**Why this priority**: The primary value of Spec 013 is fast, observable runtime validation across core subsystems.

**Independent Test**: Open each panel and verify that displayed values reflect current runtime state and update while the application runs.

**Acceptance Scenarios**:

1. **Given** DebugUI is running, **When** the Engine panel is opened, **Then** frame metrics and renderer/device details are visible and updating.
2. **Given** Scene contains nodes, **When** Scene Inspector is opened, **Then** node hierarchy is visible and selecting a node shows editable transform fields.
3. **Given** textures are loaded through TextureManager, **When** Texture Viewer is opened, **Then** texture name, resolution, and memory usage are listed.
4. **Given** camera panel values are edited, **When** parameters change, **Then** camera state updates immediately and scene view reflects changes.
5. **Given** input events occur, **When** Input Monitor is visible, **Then** current mouse and key/button states are displayed.

---

### User Story 4 - Validate End-to-End Rendering Integration (Priority: P2)

As an engine developer, I want a simple test scene that exercises VFS, texture loading, scene traversal, camera projection, sprite rendering, and debug overlay so I can verify full runtime integration.

**Why this priority**: This confirms cross-subsystem compatibility and reduces integration risk for future gameplay/content work.

**Independent Test**: Start application with test scene enabled and verify expected node content renders, debug overlay appears, and subsystem inspection panels show expected state.

**Acceptance Scenarios**:

1. **Given** the test scene initializes, **When** runtime begins, **Then** a root hierarchy with a test sprite node is created.
2. **Given** test texture path is valid in VFS, **When** test sprite initializes, **Then** texture loads and renders in scene.
3. **Given** camera and scene are active, **When** the frame renders, **Then** scene content and debug overlay both render in correct order.

---

### User Story 5 - Restrict DebugUI to Development Builds (Priority: P2)

As a release engineer, I want DebugUI compiled only for development/debug targets so production builds remain free of debug UI overhead and dependencies.

**Why this priority**: This protects release build size, performance, and dependency boundaries.

**Independent Test**: Build both development and production configurations; verify DebugUI features exist only in development configurations.

**Acceptance Scenarios**:

1. **Given** a development/debug build, **When** LXShell starts, **Then** DebugUI is available.
2. **Given** a production build, **When** LXShell starts, **Then** DebugUI functionality is disabled or excluded.
3. **Given** production build artifacts, **When** dependency boundaries are reviewed, **Then** no Engine module depends on DebugUI or Dear ImGui.

---

### Edge Cases

- What happens when DebugUI initialization fails after Engine initialized successfully?
- What happens when renderer/device context becomes invalid during runtime while DebugUI is active?
- What happens when window resize occurs while a panel is being actively interacted with?
- How are input events handled when focus rapidly alternates between DebugUI and non-UI game window regions?
- What happens when scene contains zero nodes or texture manager contains zero textures?
- What happens when test texture path is missing or unreadable in VFS?
- What happens when camera parameters are edited to invalid ranges from the Camera panel?
- What happens when development build flags are misconfigured for a production target?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST introduce a DebugUI subsystem in LXShell as the single owner of developer UI lifecycle.
- **FR-002**: The system MUST keep Engine modules UI-framework agnostic.
- **FR-003**: The system MUST ensure Engine modules do not include or depend on Dear ImGui headers.
- **FR-004**: The system MUST keep all Dear ImGui integration code in LXShell.
- **FR-005**: The system MUST enforce dependency direction `LXShell(DebugUI) -> Engine`.
- **FR-006**: The system MUST integrate Dear ImGui as a Git submodule under `Vendor/imgui`.
- **FR-007**: The system MUST use Win32 and DX11 Dear ImGui backend integrations for runtime UI.
- **FR-008**: The system MUST compile Dear ImGui core and required backend sources as part of LXShell project build configuration.
- **FR-009**: The system MUST initialize DebugUI only after Engine initialization succeeds.
- **FR-010**: DebugUI initialization MUST create and configure Dear ImGui runtime context using the active window and renderer device/context.
- **FR-011**: The system MUST log a successful DebugUI initialization event.
- **FR-012**: DebugUI render execution MUST happen after scene/sprite rendering work and before frame present.
- **FR-013**: DebugUI frame render flow MUST execute new-frame, panel draw, and draw-data submission in one deterministic sequence per frame.
- **FR-014**: The system MUST provide a DebugUI resize handler and invoke it from the existing resize chain after engine/renderer resize processing.
- **FR-015**: Window input events MUST be offered to DebugUI first.
- **FR-016**: If DebugUI consumes an input event, the event MUST NOT be forwarded to InputSystem.
- **FR-017**: If DebugUI does not consume an input event, InputSystem MUST process the event normally.
- **FR-018**: DebugUI MUST include an Engine Panel that shows FPS, frame time, renderer statistics, and GPU device information.
- **FR-019**: DebugUI MUST include a Scene Inspector that displays SceneNode hierarchy.
- **FR-020**: Scene Inspector MUST show selected node transform values (position, rotation, scale).
- **FR-021**: Scene Inspector transform edits MUST apply immediately to selected node state.
- **FR-022**: DebugUI MUST include a Texture Viewer listing loaded textures with name, resolution, and memory usage.
- **FR-023**: DebugUI MUST include a Camera Panel for active camera position, rotation, FOV, near plane, and far plane.
- **FR-024**: Camera Panel edits MUST apply immediately and trigger camera state update behavior.
- **FR-025**: DebugUI MUST include an Input Monitor displaying mouse position, mouse button states, and active key states.
- **FR-026**: LXShell runtime MUST initialize a simple test scene containing a root plus at least one test sprite node for integration validation.
- **FR-027**: Test sprite setup MUST use VFS + TextureManager asset path resolution and load from `Data/texture/test.dds`.
- **FR-028**: The system MUST log key DebugUI events including initialization, panel open events, and camera parameter updates.
- **FR-029**: DebugUI compilation MUST be restricted to development/debug build configurations.
- **FR-030**: Production build configuration MUST disable or exclude DebugUI runtime behavior.
- **FR-031**: DebugUI shutdown MUST execute before renderer destruction.
- **FR-032**: DebugUI shutdown MUST release backend state and destroy Dear ImGui context cleanly.
- **FR-033**: If DebugUI initialization fails, runtime MUST continue in a controlled degraded mode with logging, or abort startup with explicit error policy defined and applied consistently.
- **FR-034**: DebugUI panel updates MUST be read-only for non-editable metrics and writable only for explicitly editable controls.
- **FR-035**: The system MUST preserve existing Engine ownership boundaries and avoid introducing reverse dependencies from Engine to LXShell.

### Key Entities *(include if feature involves data)*

- **DebugUI**: LXShell-owned runtime subsystem that initializes, renders, and shuts down developer UI.
- **PanelSet**: Collection of panels (Engine Panel, Scene Inspector, Texture Viewer, Camera Panel, Input Monitor) managed by DebugUI.
- **EngineMetricsSnapshot**: Per-frame metrics exposed to DebugUI (FPS, frame time, renderer stats, GPU information).
- **SceneNodeViewModel**: Hierarchical representation of scene nodes and selected transform state for inspection/edit.
- **TextureInfoViewModel**: Texture list entry including name, dimensions, and memory usage.
- **CameraStateViewModel**: Editable active camera values used by camera panel.
- **InputStateViewModel**: Current mouse/keyboard state values shown in input monitor.
- **TestSceneConfig**: Development-only validation scene composition and test texture source path.
- **InputRoutingDecision**: Per-event consumed/not-consumed decision that determines whether InputSystem receives the event.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In development builds, DebugUI is visible and interactive within 2 seconds after entering the main loop in at least 95% of launches.
- **SC-002**: Panel refresh lag remains below one rendered frame under normal runtime load (no stale panel state across consecutive frames).
- **SC-003**: Input routing correctness is 100% in validation runs: consumed UI events are never processed by InputSystem, and non-consumed events are always processed.
- **SC-004**: All five required panels are available in a single session and display non-empty runtime data when corresponding subsystems are active.
- **SC-005**: Scene Inspector transform edits and Camera Panel edits produce visible runtime state changes within one frame.
- **SC-006**: Test scene validates end-to-end subsystem path (VFS, texture loading, scene traversal, camera projection, rendering) in one launch without manual code changes.
- **SC-007**: Development logs include DebugUI initialization and camera-update events for every session where those actions occur.
- **SC-008**: Production builds show zero DebugUI entry points in runtime menus and no execution of DebugUI lifecycle.
- **SC-009**: Shutdown completes without leaked DebugUI context/resources in repeated start/stop runs (minimum 10 consecutive runs).

## Out of Scope

- In-engine gameplay editor tooling beyond inspection and basic transform/camera edits.
- Persistent editor layouts, user profiles, or serialized UI preferences.
- Remote debugging, network telemetry dashboards, or multi-client inspection.
- Runtime asset authoring pipelines.
- General-purpose scripting consoles or command interpreters.

## Dependencies

- Existing Engine subsystem accessors and runtime lifecycle hooks.
- Existing resize and input callback routing in Application/Win32Window.
- Dear ImGui submodule availability in `Vendor/imgui`.
- Development build configuration macros and build pipeline support.

## Assumptions

1. LXShell is the only permitted location for DebugUI and ImGui integration code.
2. Existing Engine interfaces expose sufficient read/write access for required panel interactions.
3. Development/debug builds are clearly distinguishable by established build flags.
4. Test texture asset path (`Data/texture/test.dds`) is available in development environment or can be provisioned as part of validation setup.
5. Renderer frame lifecycle already supports adding one additional overlay render pass before present.
6. Runtime remains single-threaded for DebugUI interactions in this phase.
7. DebugUI is a developer-only aid and not part of player-facing feature scope.
