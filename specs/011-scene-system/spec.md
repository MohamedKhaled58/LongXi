# Feature Specification: Scene System

**Feature Branch**: `011-scene-system`
**Created**: 2026-03-11
**Status**: Draft
**Input**: User description: "Spec 011 — Scene System: Introduce a Scene subsystem as an Engine component that manages world objects via SceneNode hierarchy, handles hierarchical transforms, performs update and render traversals, integrates with Engine lifecycle, and delegates rendering to the DX11 renderer."

## Clarifications

### Session 2026-03-11

- Q: Should SceneNode rotation values be stored in degrees or radians? → A: Degrees — human-readable storage; renderer converts to radians internally when using DirectXMath (Assumption 2, FR-017 updated)
- Q: When should world transforms be recomputed? → A: Dirty-flag system — setting any local transform marks node dirty; during traversal, recompute world transform if node is dirty OR parent is dirty (FR-016, FR-018 updated)
- Q: Is re-parenting a SceneNode to a different parent supported in Phase 1? → A: Not supported — parent is immutable after creation; workaround is remove + re-create under new parent (Out of Scope updated, FR-012a added, Edge Cases updated)

## User Scenarios & Testing

### User Story 1 — Engine Has a Scene Subsystem (Priority: P1)

As an engine developer, I want an Engine-owned Scene object accessible via `Engine::GetScene()` so that game code has a single, stable root for the entire world.

**Why this priority**: The Scene subsystem is the prerequisite for all world representation. Without it, no world objects can exist and nothing in the game world can be updated or rendered. Every other user story in this spec depends on this.

**Independent Test**: Can be fully tested by verifying that `Engine::GetScene()` returns a valid `Scene&` after `Engine::Initialize()`, that `[Scene] Initialized` appears in the log, and that Engine shutdown produces no memory errors.

**Acceptance Scenarios**:

1. **Given** Engine initializes successfully, **When** `Engine::GetScene()` is called, **Then** a valid `Scene` reference is returned
2. **Given** Engine initializes, **When** the log is inspected, **Then** `[Scene] Initialized` appears as the last initialization step
3. **Given** Engine shuts down, **When** all subsystems are destroyed, **Then** Scene is destroyed before TextureManager and no memory errors occur
4. **Given** `Engine::Initialize()` is called, **When** Scene initialization is attempted, **Then** Scene initializes successfully and Engine::Initialize() returns true

---

### User Story 2 — Add and Remove World Objects as Scene Nodes (Priority: P1)

As an engine developer, I want to add world objects to the scene as SceneNode instances and have the Scene own and manage their lifetime so that I can build a structured world without managing object memory manually.

**Why this priority**: Node creation and destruction is the fundamental mechanism for world composition. All game objects — maps, characters, particles, UI — will be added via this mechanism.

**Independent Test**: Can be tested by creating a SceneNode, adding it to the scene root, verifying it appears in the hierarchy, then removing it and verifying it is destroyed without leaks.

**Acceptance Scenarios**:

1. **Given** a SceneNode is created, **When** `Scene::AddNode(node)` is called, **Then** the node is attached to the scene root and the log shows `[Scene] Node added`
2. **Given** a node exists in the scene, **When** `Scene::RemoveNode(node)` is called, **Then** the node is detached and destroyed, and the log shows `[Scene] Node removed`
3. **Given** a parent node exists in the scene, **When** a child node is added via `parent.AddChild(child)`, **Then** the child's lifetime is owned by the parent
4. **Given** a parent node is removed from the scene, **When** destruction occurs, **Then** all child nodes are recursively destroyed

---

### User Story 3 — Scene Nodes Update Each Frame (Priority: P1)

As an engine developer, I want all scene nodes to be updated each frame via a depth-first traversal so that world objects can apply game logic (movement, animation state, AI) independently without the Engine knowing about each object type.

**Why this priority**: Per-frame update is the core game loop integration. Without it, the scene is a static data structure with no behavior.

**Independent Test**: Can be tested by adding a custom SceneNode subclass that increments a counter in its `Update()` override, running the engine for N frames, and verifying the counter equals N.

**Acceptance Scenarios**:

1. **Given** a SceneNode is in the scene, **When** `Engine::Update()` is called each frame, **Then** `SceneNode::Update(deltaTime)` is called on every node in the hierarchy
2. **Given** a parent node has two children, **When** `Scene::Update()` traverses the tree, **Then** parent is updated before children (pre-order depth-first)
3. **Given** deltaTime is measured, **When** `SceneNode::Update(deltaTime)` is called, **Then** the deltaTime value accurately reflects elapsed time since last frame in seconds

---

### User Story 4 — Renderable Nodes Submit Draw Calls to Renderer (Priority: P1)

As an engine developer, I want renderable SceneNode subclasses to submit draw commands to the renderer during a separate render traversal so that rendering is fully decoupled from game logic and Scene never contains draw code directly.

**Why this priority**: Render submission is what makes the scene visible. The separation of update and render traversals is an architectural invariant: Scene orchestrates traversal, nodes issue draw calls, Scene never draws directly.

**Independent Test**: Can be tested by creating a concrete renderable node subclass (e.g., SpriteNode), adding it to the scene, running one frame, and verifying the expected draw call is issued to the renderer without Scene itself containing any rendering logic.

**Acceptance Scenarios**:

1. **Given** a renderable SceneNode subclass is in the scene, **When** `Scene::Render(renderer)` is called, **Then** `SceneNode::Submit(renderer)` is called on each renderable node
2. **Given** a non-renderable SceneNode is in the scene, **When** render traversal occurs, **Then** the base `Submit()` is a no-op and no draw command is issued
3. **Given** the render traversal runs, **When** inspecting Scene source code, **Then** Scene contains zero direct draw calls — all draws are issued by node `Submit()` implementations
4. **Given** a parent node and a child renderable node, **When** render traversal occurs, **Then** parent Submit is called before child Submit (pre-order depth-first)

---

### User Story 5 — Hierarchical Transforms Propagate from Parent to Child (Priority: P2)

As an engine developer, I want each SceneNode to have a local transform (position, rotation, scale) and a computed world transform derived from the parent's world transform so that moving a parent automatically repositions all its children in world space.

**Why this priority**: Hierarchical transforms are essential for character rigs, mounted objects, and grouped geometry. Without world transform computation, sub-object rendering is impossible. This is a P2 because the scene functions without it (nodes exist and update), but renderable nodes cannot be positioned correctly without it.

**Independent Test**: Can be tested by placing a parent node at position (10, 0, 0) and a child at local position (5, 0, 0), then verifying the child's world position is (15, 0, 0).

**Acceptance Scenarios**:

1. **Given** a node has local position (5, 0, 0) and its parent has world position (10, 0, 0), **When** world transform is computed, **Then** the node's world position is (15, 0, 0)
2. **Given** a node has local scale (2, 2, 2) and its parent has world scale (3, 3, 3), **When** world transform is computed, **Then** the node's world scale is (6, 6, 6)
3. **Given** a parent node's position changes, **When** the next update occurs, **Then** all child world transforms are recomputed to reflect the new parent position
4. **Given** a root node has no parent, **When** world transform is computed, **Then** world transform equals local transform

---

### User Story 6 — Scene Handles Window Resize Events (Priority: P2)

As an engine developer, I want the Scene to receive and forward window resize events so that renderable nodes that depend on viewport dimensions (e.g., UI nodes, camera nodes) can respond to window size changes without polling.

**Why this priority**: Resize handling ensures that screen-space renderable nodes remain correct when the window dimensions change. Without it, viewport-dependent nodes produce incorrect output after a resize.

**Independent Test**: Can be tested by triggering a resize event and verifying that `Scene::OnResize(newWidth, newHeight)` is called and forwarded to any registered nodes that implement resize handling.

**Acceptance Scenarios**:

1. **Given** the window is resized, **When** `Engine::OnResize()` is called, **Then** `Scene::OnResize(width, height)` is called as part of the resize chain
2. **Given** `Scene::OnResize()` is called with valid dimensions, **When** the call is processed, **Then** any nodes that implement `OnResize` receive the updated dimensions
3. **Given** `Scene::OnResize()` is called with zero or negative dimensions, **When** the call is processed, **Then** the resize is ignored and no nodes are notified

---

### Edge Cases

- What happens when `Scene::AddNode()` is called with a null pointer?
- What happens when `Scene::RemoveNode()` is called with a node not in the scene?
- What happens when a node's `Update()` throws or crashes — does the traversal stop or continue?
- What happens when `Scene::Render()` is called before `Scene::Update()` in a frame?
- What happens when a node adds a child during its own `Update()` call (structural mutation during traversal)?
- What happens when a node removes itself or a sibling during traversal?
- What happens when the scene root has zero children and `Update()` or `Render()` is called?
- What happens when a child node is re-parented to a different parent? → **Not supported in Phase 1.** A node's parent is immutable after `AddChild()`. To move a node to a different parent, the node must be removed and destroyed, then a new equivalent node created under the new parent. Calling any re-parenting operation on a node is undefined behavior.
- What happens if `Scene::OnResize()` is called during an active update traversal?
- What happens when a scene node has no transform (default transform)?

---

## Requirements

### Functional Requirements

- **FR-001**: The system MUST introduce a `Scene` class as an Engine subsystem, owned by Engine and initialized after TextureManager
- **FR-002**: The system MUST expose `Engine::GetScene()` returning a valid `Scene&` when Engine is initialized
- **FR-003**: The system MUST initialize Scene as Engine initialization step 6, in order: Renderer → InputSystem → VirtualFileSystem → TextureManager → SpriteRenderer → Scene
- **FR-004**: The system MUST destroy Scene during Engine shutdown before SpriteRenderer (reverse initialization order)
- **FR-005**: The system MUST call `Scene::Update(deltaTime)` from `Engine::Update()` after InputSystem update
- **FR-006**: The system MUST call `Scene::Render(renderer)` from `Engine::Render()` after the scene placeholder comment, before SpriteRenderer Begin/End
- **FR-007**: The system MUST call `Scene::OnResize(width, height)` from `Engine::OnResize()` after Renderer and SpriteRenderer resize
- **FR-008**: The system MUST introduce a `SceneNode` base class with a local transform (position, rotation, scale as `Vector3`)
- **FR-009**: The system MUST add `Vector3` (float x, y, z) to `LongXi/LXEngine/Src/Math/Math.h` alongside `Vector2` and `Color`
- **FR-010**: The system MUST give `SceneNode` a `SceneNode* m_Parent` non-owning pointer (null for root-level nodes)
- **FR-011**: The system MUST give `SceneNode` a `std::vector<std::unique_ptr<SceneNode>> m_Children` for owning child nodes
- **FR-012**: The system MUST provide `SceneNode::AddChild(std::unique_ptr<SceneNode>)` which takes ownership, sets the child's parent pointer, and logs `[Scene] Node added`. The parent pointer is set once at the time of `AddChild()` and is immutable for the lifetime of the node.
- **FR-012a**: The system MUST NOT provide a `SetParent()` or `ReparentTo()` method on `SceneNode`. Dynamic re-parenting is explicitly out of scope for Phase 1. If a node must appear under a different parent, it must be removed and destroyed, then a new node created under the new parent.
- **FR-013**: The system MUST provide `SceneNode::RemoveChild(SceneNode*)` which releases ownership, destroys the child, and logs `[Scene] Node removed`
- **FR-014**: The system MUST provide `virtual void SceneNode::Update(float deltaTime)` as an overridable no-op base implementation
- **FR-015**: The system MUST provide `virtual void SceneNode::Submit(DX11Renderer& renderer)` as an overridable no-op base implementation
- **FR-016**: The system MUST give `SceneNode` a computed world transform (`Vector3 m_WorldPosition`, `Vector3 m_WorldRotation`, `Vector3 m_WorldScale`) and a `bool m_TransformDirty` flag. Setting any local transform component (position, rotation, or scale) MUST set `m_TransformDirty = true` on the node.
- **FR-017**: The system MUST compute world transform by combining parent world transform with local transform: world position = parent world position + local position; world scale = parent world scale × local scale; world rotation = parent world rotation + local rotation (Euler additive in degrees for Phase 1). Rotation values are stored and computed in degrees; conversion to radians is the responsibility of renderer/shader code, not SceneNode.
- **FR-018**: The system MUST use a dirty-flag recomputation strategy during scene traversal: when processing a node, if the node's `m_TransformDirty` is true OR the parent's `m_TransformDirty` is true, the node's world transform is recomputed from the parent's world transform and the node's local transform. After recomputing, `m_TransformDirty` is cleared on the node. Nodes whose parents are dirty are always recomputed regardless of their own dirty flag, ensuring correct propagation down the hierarchy.
- **FR-019**: The system MUST give `Scene` a root node (`SceneNode m_Root`) that serves as the parent of all top-level nodes
- **FR-020**: The system MUST provide `Scene::AddNode(std::unique_ptr<SceneNode>)` which attaches the node to the scene root
- **FR-021**: The system MUST provide `Scene::RemoveNode(SceneNode*)` which detaches and destroys the specified node from the root
- **FR-022**: The system MUST guard `Scene::AddNode()` against null pointers — null nodes must be ignored with an error log
- **FR-023**: The system MUST guard `Scene::RemoveNode()` against null pointers and nodes not found in the root — both must be ignored with an error log
- **FR-024**: The system MUST implement `Scene::Update(float deltaTime)` as a pre-order depth-first traversal calling `SceneNode::Update()` on every node, parent before children
- **FR-025**: The system MUST implement `Scene::Render(DX11Renderer& renderer)` as a pre-order depth-first traversal calling `SceneNode::Submit()` on every node, parent before children
- **FR-026**: The system MUST implement `Scene::OnResize(int width, int height)` which guards against zero/negative dimensions and forwards to any nodes that implement resize notification
- **FR-027**: The system MUST NOT contain any direct rendering code in `Scene` — all draw calls are issued by `SceneNode::Submit()` implementations
- **FR-028**: The system MUST log `[Scene] Initialized` after successful initialization
- **FR-029**: The system MUST log `[Scene] Node added` when a node is added to the hierarchy
- **FR-030**: The system MUST log `[Scene] Node removed` when a node is removed from the hierarchy
- **FR-031**: The system MUST place `Scene` and `SceneNode` source files in `LongXi/LXEngine/Src/Scene/`
- **FR-032**: The system MUST place `Scene` and `SceneNode` in the `LongXi` namespace (no nested namespaces)
- **FR-033**: The system MUST add `Scene.h` to the LXEngine public entry point header (`LXEngine.h`)
- **FR-034**: The system MUST measure deltaTime using a monotonic clock between Engine::Update() calls and pass it in seconds (float) to Scene::Update()
- **FR-035**: The system MUST guard Scene::Update() and Scene::Render() against structural mutations during traversal — nodes added or removed during traversal must be deferred until traversal completes or the traversal must operate on a snapshot

### Key Entities

- **Scene**: Engine subsystem that owns the root node and coordinates all world object update and render traversals. Has no direct rendering code. Provides AddNode/RemoveNode for top-level node management.
- **SceneNode**: Base class for all world objects. Stores local and world transforms, owns child nodes, provides virtual Update() and Submit() overrides. Non-renderable nodes have no-op Submit().
- **Transform (local)**: Per-node position, rotation, scale stored as `Vector3` (rotation in degrees). Defines the node's position relative to its parent. Setting any component marks the node's `m_TransformDirty` flag.
- **World Transform**: Computed transform combining all ancestor local transforms. Defines the node's actual position, rotation, and scale in world space.
- **Vector3**: New shared math type (float x, y, z) added to `Math/Math.h`. Used for all 3D transform components.
- **DX11Renderer** (existing): Passed to `Scene::Render()` and forwarded to each `SceneNode::Submit()`. Nodes use it to issue draw calls. Scene itself does not call any renderer methods.

## Success Criteria

### Measurable Outcomes

- **SC-001**: `Engine::GetScene()` returns a valid reference after `Engine::Initialize()` succeeds
- **SC-002**: `[Scene] Initialized` appears in log after Engine initialization
- **SC-003**: A SceneNode added via `Scene::AddNode()` has its `Update()` called on every subsequent `Engine::Update()` call
- **SC-004**: A renderable SceneNode subclass has its `Submit()` called on every `Engine::Render()` call
- **SC-005**: Scene contains zero direct draw calls — verified by code inspection of `Scene.cpp`
- **SC-006**: A child node's world position equals parent world position + child local position (verified numerically)
- **SC-007**: Removing a parent node removes all its children — verified by node count before and after removal
- **SC-008**: Engine initializes and runs without crashes with an empty scene (zero nodes)
- **SC-009**: Engine shutdown produces no memory errors or live object warnings with nodes in the scene
- **SC-010**: `Scene::OnResize()` is called when `Engine::OnResize()` fires — verified by log or test node
- **SC-011**: Scene's deltaTime value matches elapsed wall-clock time within 10% for a known frame duration

## Out of Scope

The following items are explicitly OUT of scope for this specification:

- **3D rendering**: Mesh loading, skeletal animation, material system, lighting (deferred to future specs)
- **Camera system**: Camera node, view/projection matrices, frustum culling (deferred to future spec)
- **Map system**: 3DGameMap loading, tile rendering, map chunks (deferred to future spec)
- **Character system**: 3DRole, character controllers, animation state machines (deferred to future spec)
- **Particle system**: Emitter nodes, particle update (deferred to future spec)
- **Collision / physics**: Spatial queries, bounding volumes, collision response (deferred to future spec)
- **Quaternion rotation**: Full quaternion-based rotation transforms (deferred — Phase 1 uses Euler angles)
- **Matrix math**: 4×4 transformation matrices, view/projection matrices (deferred to camera spec)
- **Scene serialization**: Saving/loading scene state from files (deferred to future spec)
- **Scene graph queries**: Find-by-name, find-by-type, spatial queries (deferred to future spec)
- **Component system**: Entity-component-system (ECS) or data-oriented design (out of scope for Phase 1)
- **Scripting integration**: Lua, Python, or other scripting language hooks (out of scope)
- **Multi-threaded traversal**: Parallel node update or job system (deferred — Phase 1 is single-threaded)
- **Scene layers / visibility**: Culling layers, visibility flags, camera-relative culling (deferred to future spec)
- **Node re-parenting**: Moving a SceneNode from one parent to another while both remain in the scene — not supported in Phase 1. Parent is immutable after creation. Deferred to a later phase once the scene system is stable.
- **Networking / replication**: Scene state synchronization over network (deferred to multiplayer spec)
- **Concrete node types**: SpriteNode, MeshNode, ParticleNode implementation (deferred — this spec only defines the base class)

## Assumptions

1. **Vector3 placement**: `Vector3` is added to the existing `LongXi/LXEngine/Src/Math/Math.h` header alongside `Vector2` and `Color`. No new file or module is created for math types.
2. **Euler rotation for Phase 1**: Rotation is stored as Euler angles (pitch, yaw, roll) in **degrees**. World rotation is computed by additive combination of parent and local rotation in degrees. Conversion to radians is handled by renderer/shader code when interfacing with DirectXMath or GPU pipeline. Full quaternion support is deferred.
3. **Dirty-flag world transform**: World transform uses a dirty-flag strategy. Setting any local transform component marks the node dirty. During traversal, world transform is recomputed only for dirty nodes or nodes whose parent is dirty. After recomputation, the dirty flag is cleared. New nodes are created with `m_TransformDirty = true` so their world transform is computed on the first traversal. World transform computation uses component-wise addition for position and rotation (in degrees), and component-wise multiplication for scale. No matrix multiplication is required in Phase 1.
4. **DX11Renderer passed by reference**: `Scene::Render()` and `SceneNode::Submit()` receive `DX11Renderer&` directly, consistent with the existing Engine subsystem accessor pattern. Scene does not store a persistent reference to the renderer.
5. **Scene initialization is non-fatal**: If Scene initialization fails for any reason, Engine continues without Scene (consistent with SpriteRenderer non-fatal pattern from Spec 010/011).
6. **deltaTime from Engine**: The Engine measures frame time using a monotonic clock (e.g., `std::chrono::high_resolution_clock`) and passes it to `Scene::Update()`. No separate timer subsystem is introduced.
7. **Scene render order relative to SpriteRenderer**: `Scene::Render()` is called BEFORE `SpriteRenderer::Begin()/End()` so that 3D scene geometry renders before 2D screen-space sprites/UI.
8. **Root node is internal**: `Scene::m_Root` is not exposed publicly. Top-level nodes are added via `Scene::AddNode()` which attaches them to the root.
9. **No traversal mutation guard in Phase 1**: Structural mutation during traversal (adding/removing nodes inside Update()) is undefined behavior in Phase 1. FR-035 requires this to be documented but the enforcement mechanism is deferred to planning.
10. **Logging macros**: Scene uses existing `LX_ENGINE_INFO` and `LX_ENGINE_ERROR` macros from `LogMacros.h`.
11. **Engine hook points**: Engine already has stub `UpdateScene()` and `RenderScene()` methods from the Spec 009 merge. These stubs will be replaced by actual `m_Scene->Update()` and `m_Scene->Render()` calls.
12. **No external test harness**: Testing is manual — the test application in LXShell is updated to add a SceneNode and verify update/render calls via log output.
13. **SpriteRenderer already exists**: Spec 011 (SpriteRenderer, misnamed as Spec 010 in the branch) is already merged to master. Scene integrates alongside it without modifying SpriteRenderer.

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
