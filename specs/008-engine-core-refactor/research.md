# Research: Engine Core Refactor and Engine Service Access

**Feature**: Spec 008 - Engine Core Refactor
**Date**: 2026-03-10
**Status**: Complete - No external research required

## Overview

This is a **pure architectural refactor** with well-defined technologies established in the project constitution (Article III). No external technology research or pattern exploration is required.

## Technology Stack (from Constitution)

All technical decisions are **binding per Article III** of the constitution:

| Aspect | Technology | Source |
|--------|-----------|--------|
| Language | C++23 (MSVC v145) | Article III, Section: Toolchain |
| Platform | Windows-only | Article III, Section: Platform and Tooling |
| Windowing | Raw Win32 API | Article III, Section: Platform and Tooling |
| Renderer | DirectX 11 | Article III, Section: Platform and Tooling |
| Build System | Premake5 + Visual Studio 2026 | Article III, Section: Platform and Tooling |
| Architecture | Static libraries (LXCore/LXEngine/LXShell) | Article III, Section: Module Structure |
| Threading Model | Single-threaded (Phase 1) | Article IX, Section: Phase 1 Runtime Policy |

## Design Decisions

### Decision: Engine Class Ownership Pattern

**Choice**: Central Engine class owning all subsystems via `std::unique_ptr`

**Rationale**:
- **Constitutional compliance**: Eliminates global state (Article IV - State and Coupling)
- **Proven pattern**: Industry standard for game engine architecture (Unreal's UEngine, Unity's UnityEngine, Godot's Engine)
- **Dependency injection**: Subsystems receive `Engine&` reference to access other services via getters
- **Explicit dependencies**: No hidden global state - all access paths visible through Engine getters
- **Future-ready**: Compatible with future multithreading (Article IX - MT-Aware Architecture)

**Alternatives Considered**:
- Service locator pattern with global accessor - **Rejected**: Violates Article IV (hidden global state)
- Dependency injection container - **Rejected**: Premature abstraction (Article IV - Abstraction Discipline)
- Singleton pattern for subsystems - **Rejected**: Violates Article IV (hidden global state)

### Decision: Reference Passing to Subsystems

**Choice**: Subsystems receive `Engine&` (non-owning reference) via constructor

**Rationale**:
- **Explicit lifetime**: Engine owns subsystems, subsystems hold non-owning reference back
- **No circular ownership**: Prevents memory leaks and dangling pointers
- **Compile-time safety**: Reference cannot be null (vs raw pointer)
- **Simple and direct**: No smart pointer overhead for non-owning reference
- **Future-ready**: Compatible with future multithreading if Engine lifetime exceeds subsystem access

**Alternatives Considered**:
- `std::shared_ptr<Engine>` - **Rejected**: Implies shared ownership (incorrect ownership model)
- `Engine*` raw pointer - **Rejected**: Null-safety risk (reference is safer)
- Global accessor `Engine::Get()` - **Rejected**: Violates Article IV (hidden global state)

### Decision: Initialization Order

**Choice**: Renderer → InputSystem → VirtualFileSystem → TextureManager (dependency order)

**Rationale**:
- **TextureManager depends on**: Renderer (GPU resources) + VFS (file access)
- **InputSystem is independent**: No subsystem dependencies, can initialize early
- **VFS is independent**: No subsystem dependencies, can initialize early
- **Shutdown is reverse order**: TextureManager → VFS → InputSystem → Renderer

**Dependency Graph**:
```
TextureManager
  ├─ depends on → Renderer (CreateTexture)
  └─ depends on → VFS (LoadTexture)

All other subsystems are independent.
```

### Decision: Non-Const Getter Methods

**Choice**: Engine provides non-const reference getters: `GetRenderer()`, `GetInput()`, `GetVFS()`, `GetTextureManager()`

**Rationale**:
- **Subsystem services require mutable access**: e.g., `TextureManager::LoadTexture()` is non-const
- **Engine is mutable runtime object**: Not a value type or immutable configuration
- **Simplicity**: No complexity distinction between "query" vs "mutate" access
- **Industry standard**: Game engines typically provide mutable access to subsystems

**Alternatives Considered**:
- Const getters with `const_cast` - **Rejected**: Undefined behavior, dangerous
- Separate `Get()` (const) and `GetMutable()` - **Rejected**: Unnecessary complexity for this use case
- `std::shared_ptr` returns - **Rejected**: Implies shared ownership (incorrect)

## Integration Points

### Existing Subsystem Dependencies

**TextureManager** (already exists):
```cpp
// Current constructor (before refactor):
TextureManager(CVirtualFileSystem& vfs, DX11Renderer& renderer)

// After refactor (receives Engine& instead):
TextureManager(Engine& engine)
// Internally calls:
//   - engine.GetRenderer() for DX11Renderer&
//   - engine.GetVFS() for CVirtualFileSystem&
```

**Rationale**: Maintains existing dependency pattern but routes through Engine for centralized access control.

### Application Integration

**Before** (current state):
```cpp
class Application {
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<DX11Renderer> m_Renderer;
    std::unique_ptr<InputSystem> m_InputSystem;
    std::unique_ptr<CVirtualFileSystem> m_VirtualFileSystem;
    std::unique_ptr<TextureManager> m_TextureManager;
};
```

**After** (target state):
```cpp
class Application {
    std::unique_ptr<Win32Window> m_Window;
    std::unique_ptr<Engine> m_Engine;
};
```

**Rationale**: Application reduced from 6 to 2 members (per SC-001), delegates all subsystem management to Engine.

## Performance Considerations

### Zero Overhead Design Goals

**Per Assumption #12 in spec**: No performance regression vs current implementation

**Analysis**:
- **Getter calls**: Inline function calls (optimized away by compiler)
- **Reference passing**: No memory allocation, no indirection vs current direct ownership
- **Unique_ptr overhead**: Same as current implementation (already using unique_ptr)
- **No virtual dispatch**: All concrete types, no polymorphism overhead

**Expected Impact**: Zero runtime overhead (compile-time indirection only)

## Risk Assessment

### Low Risk Items

✅ **Pure refactor**: No new features, only moving ownership
✅ **Well-defined tech stack**: All technologies from constitution
✅ **Backward compatibility**: Existing subsystems retain functionality
✅ **No data model changes**: TextureManager, VFS, Renderer interfaces unchanged
✅ **Single-threaded**: No threading complexity (Article IX compliance)

### Medium Risk Items

⚠️ **Application changes**: TestApplication must be updated to use Engine getters
⚠️ **Window resize timing**: WM_SIZE during Engine initialization (Assumption #7)

**Mitigation**:
- Manual testing per Assumption #11
- Resize events already handled by current Application::OnResize()

### No High Risk Items

All changes are localized to LXEngine module, no cross-cutting concerns.

## Open Questions

**None**. Specification is comprehensive and implementation-ready.

## References

- **Constitution**: `.specify/memory/constitution.md`
- **Specification**: `specs/008-engine-core-refactor/spec.md`
- **Existing Code**:
  - `LongXi/LXEngine/Src/Application/Application.h`
  - `LongXi/LXEngine/Src/Texture/TextureManager.h`
  - `LongXi/LXEngine/Src/Renderer/DX11Renderer.h`

## Reference Implementation Rule
- The agent must inspect reference implementations located in D:\Yamen Development\Old-Reference\cqClient\Conquer.
- Relevant files may include renderer, viewport, pipeline, and device initialization code.
- The reference code must be used only to understand behavior and constraints.
- The new architecture must follow the LongXi engine design.
