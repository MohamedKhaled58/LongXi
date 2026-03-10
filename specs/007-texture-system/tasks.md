# Tasks: Texture System

**Input**: Design documents from `/specs/007-texture-system/`
**Prerequisites**: plan.md ✅ spec.md ✅ research.md ✅ data-model.md ✅ contracts/ ✅ quickstart.md ✅

**Tests**: Not requested — no test tasks generated.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story?] Description — file path`

- **[P]**: Can run in parallel (different files, no blocking dependencies)
- **[US1/US2/US3]**: Which user story this task belongs to

---

## Phase 1: Setup

**Purpose**: Confirm build infrastructure before writing any new files.

- [X] T001 Confirm LXEngine/premake5.lua uses `Src/**.h` / `Src/**.cpp` glob and LXEngine already links `d3d11` / `dxgi` / `dxguid` — new Texture/ files and DX11 texture APIs require no premake changes in LongXi/LXEngine/premake5.lua

**Checkpoint**: Build system verified — no premake changes needed.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define pure-data header types with zero DX11 dependencies. Must complete before any user story work begins.

**CRITICAL**: All user story phases depend on these two headers.

- [X] T002 [P] Create TextureFormat enum class with values `RGBA8`, `DXT1`, `DXT3`, `DXT5` inside `namespace LongXi`; zero includes beyond `<cstdint>` if needed; zero DX11 dependencies in LongXi/LXEngine/Src/Texture/TextureFormat.h
- [X] T003 [P] Create TextureData struct with fields `Width: uint32_t`, `Height: uint32_t`, `Format: TextureFormat`, `Pixels: vector<uint8_t>` inside `namespace LongXi`; include `TextureFormat.h`, `<cstdint>`, `<vector>` in LongXi/LXEngine/Src/Texture/TextureData.h

**Checkpoint**: Both headers compile independently. No DX11 symbols in either file.

---

## Phase 3: User Story 1 — Texture Load from VFS (Priority: P1) 🎯 MVP

**Goal**: Any engine system can load a texture by virtual path and receive a GPU-ready `shared_ptr<Texture>`. Works for DDS (DXT1/3/5 and RGBA8) and TGA (24-bit and 32-bit). Textures are served from both WDF archives and loose directories via VFS.

**Independent Test**: `Application::GetTextureManager().LoadTexture("texture/terrain/grass.dds")` returns a non-null `shared_ptr<Texture>` when a valid DDS file is mounted in VFS. Startup log shows `[Texture] Loaded: texture/terrain/grass.dds`.

### Implementation for User Story 1

- [X] T004 [P] [US1] Create TextureLoader class declaration with `static bool LoadDDS(const vector<uint8_t>& data, TextureData& out)` and `static bool LoadTGA(const vector<uint8_t>& data, TextureData& out)`; include `TextureData.h`; `namespace LongXi` in LongXi/LXEngine/Src/Texture/TextureLoader.h
- [X] T005 [P] [US1] Extend DX11Renderer.h — add `#include "Texture/TextureFormat.h"`, add `using RendererTextureHandle = Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>` type alias, add public method `RendererTextureHandle CreateTexture(uint32_t width, uint32_t height, TextureFormat format, const void* pixels)` in LongXi/LXEngine/Src/Renderer/DX11Renderer.h
- [X] T006 [US1] Implement TextureLoader.cpp — `LoadDDS`: validate magic `"DDS "` at bytes 0–3, parse DDS_HEADER at offset 4 (124 bytes), detect FOURCC `DXT1/DXT3/DXT5` or `DDPF_RGB|DDPF_ALPHAPIXELS` with 32 bpp for RGBA8, read pixel data from byte 128, copy into TextureData::Pixels; `LoadTGA`: parse 18-byte header, accept image type 2 (uncompressed) or 10 (RLE), accept 24-bit (expand to RGBA8 with 0xFF alpha) or 32-bit, RLE decompress for type 10, flip rows bottom-to-top; both methods return false without modifying `out` on any failure in LongXi/LXEngine/Src/Texture/TextureLoader.cpp
- [X] T007 [US1] Implement DX11Renderer::CreateTexture in DX11Renderer.cpp — map TextureFormat to DXGI_FORMAT (RGBA8→R8G8B8A8_UNORM, DXT1→BC1_UNORM, DXT3→BC2_UNORM, DXT5→BC3_UNORM), compute rowPitch (RGBA8: width×4; DXT1: max(1,(w+3)/4)×8; DXT3/DXT5: max(1,(w+3)/4)×16), call `m_Device->CreateTexture2D` with IMMUTABLE usage + SRV bind flag, call `m_Device->CreateShaderResourceView`, immediately release Texture2D handle so SRV holds sole reference, log LX_ENGINE_ERROR on FAILED(hr) and return empty ComPtr in LongXi/LXEngine/Src/Renderer/DX11Renderer.cpp
- [X] T008 [US1] Create Texture class — `m_Handle: RendererTextureHandle`, `m_Width`, `m_Height`, `m_Format`; `~Texture() = default` (ComPtr auto-releases SRV and Texture2D on destruction); private constructor taking `(RendererTextureHandle, uint32_t, uint32_t, TextureFormat)`; `friend class TextureManager`; public getters `GetWidth/GetHeight/GetFormat/GetHandle`; include `DX11Renderer.h`; delete copy; `namespace LongXi` in LongXi/LXEngine/Src/Texture/Texture.h and LongXi/LXEngine/Src/Texture/Texture.cpp
- [X] T009 [US1] Create TextureManager.h — class declaration with constructor `TextureManager(CVirtualFileSystem& vfs, DX11Renderer& renderer)`, private members `m_Vfs: CVirtualFileSystem&`, `m_Renderer: DX11Renderer&`, `m_Cache: unordered_map<string, shared_ptr<Texture>>`; public methods `LoadTexture`, `GetTexture`, `ClearCache`; private `Normalize`; include `Texture.h`, `<unordered_map>`, `<string>`, forward-declare `CVirtualFileSystem`; `namespace LongXi` in LongXi/LXEngine/Src/Texture/TextureManager.h
- [X] T010 [US1] Implement TextureManager.cpp — `Normalize(path)`: 8-step algorithm (backslash→slash, lowercase, collapse //, strip leading slash, strip trailing slash, reject `..` segments logging WARN, collapse `.` segments, return empty if result empty); `LoadTexture`: normalize→cache lookup (INFO hit if found)→VFS ReadAll→empty check (ERROR not found)→extension detect `.dds`/`.tga` (ERROR unsupported)→TextureLoader decode (ERROR decode fail)→`m_Renderer.CreateTexture` (ERROR GPU fail)→make_shared<Texture>→insert cache→INFO "[Texture] Loaded: path (WxH FMT)"→return; `GetTexture`: normalize→cache lookup→return or nullptr (no log); `ClearCache`: clear + INFO "[Texture] Cache cleared (N entries released)" in LongXi/LXEngine/Src/Texture/TextureManager.cpp
- [X] T011 [P] [US1] Update Application.h — add `class TextureManager;` forward declaration; add `std::unique_ptr<TextureManager> m_TextureManager` private member; add `const TextureManager& GetTextureManager() const` public accessor; add `bool CreateTextureManager()` private factory in LongXi/LXEngine/Src/Application/Application.h
- [X] T012 [P] [US1] Update LXEngine.h — add `#include "Texture/TextureManager.h"` after existing includes to expose TextureManager as part of the LXEngine public API in LongXi/LXEngine/LXEngine.h
- [X] T013 [US1] Update Application.cpp — add `#include "Texture/TextureManager.h"`; implement `CreateTextureManager()` constructing `std::make_unique<TextureManager>(*m_VirtualFileSystem, *m_Renderer)` + log; call `CreateTextureManager()` in `Initialize()` after `CreateVirtualFileSystem()` with failure handling; add `m_TextureManager.reset()` as the FIRST reset in `Shutdown()` (before VFS, before Renderer — GPU resources must release while device is still alive) in LongXi/LXEngine/Src/Application/Application.cpp

**Checkpoint**: Application compiles and starts. Log shows `[Texture]` mount messages. `GetTextureManager().LoadTexture("valid/path.dds")` returns non-null `shared_ptr<Texture>` for a mounted DDS file.

---

## Phase 4: User Story 2 — Cache Hit (Priority: P2)

**Goal**: Repeated `LoadTexture` calls for the same path return the identical cached `shared_ptr<Texture>` without re-decoding or re-uploading. Case-variant and slash-variant paths resolve to the same cache entry.

**Independent Test**: Call `LoadTexture("Texture/Terrain/Grass.DDS")` twice — both calls return the same `Texture*`. Second call produces `[Texture] Cache hit:` log. One GPU upload total.

### Implementation for User Story 2

- [X] T014 [US2] Verify TextureManager::LoadTexture cache-hit path in LongXi/LXEngine/Src/Texture/TextureManager.cpp — confirm: (1) cache lookup uses normalized path key, (2) on hit, `shared_ptr` from cache is returned immediately without calling `m_Vfs.ReadAll`, (3) `LX_ENGINE_INFO("[Texture] Cache hit: {}", normalized)` is logged; correct if needed
- [X] T015 [US2] Verify TextureManager::Normalize produces a lowercase forward-slash no-leading-slash path in LongXi/LXEngine/Src/Texture/TextureManager.cpp — confirm: `"Texture\\Terrain\\Grass.DDS"` normalizes to `"texture/terrain/grass.dds"`, `/texture/terrain/grass.dds` normalizes to `"texture/terrain/grass.dds"`, and `"INI//itemtype.dat"` normalizes to `"ini/itemtype.dat"`; correct if needed

**Checkpoint**: US1 and US2 independently testable. Path normalization ensures cache always matches regardless of caller casing.

---

## Phase 5: User Story 3 — Error Safety (Priority: P3)

**Goal**: Missing files, unsupported formats, and corrupt data all return `nullptr` gracefully without crashing the engine.

**Independent Test**: `LoadTexture("nonexistent.dds")` returns `nullptr` and logs `[Texture] ERROR File not found:`. `LoadTexture("file.bmp")` returns `nullptr` and logs `[Texture] ERROR Unsupported format:`. Neither call throws or crashes.

### Implementation for User Story 3

- [X] T016 [US3] Verify all error return paths in TextureManager::LoadTexture in LongXi/LXEngine/Src/Texture/TextureManager.cpp — confirm each of these returns `nullptr` with correct log: empty path (WARN), VFS ReadAll returns empty (ERROR file not found), unknown extension (ERROR unsupported format), TextureLoader returns false (ERROR failed to decode), CreateTexture returns null ComPtr (ERROR GPU upload failed); no path throws or crashes; correct if needed
- [X] T017 [US3] Verify TextureLoader decode failure returns false correctly for invalid input in LongXi/LXEngine/Src/Texture/TextureLoader.cpp — confirm: DDS with wrong magic ("XXXX") returns false, DDS with unsupported pixel format returns false, TGA with unsupported image type (e.g. type 1 indexed) returns false, empty input vector returns false; `out` is unchanged in all failure cases; correct if needed

**Checkpoint**: All three user stories independently functional. Error safety confirmed.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Build verification, formatting, and integration confirmation.

- [X] T018 Build full solution using `Win-Build Project.bat` — resolve any compile errors; verify zero errors, zero warnings related to new texture files
- [X] T019 [P] Run `Win-Format Code.bat` to auto-format all new and modified files: TextureFormat.h, TextureData.h, TextureLoader.h/.cpp, Texture.h/.cpp, TextureManager.h/.cpp, DX11Renderer.h/.cpp, Application.h/.cpp, LXEngine.h
- [X] T020 Start application and inspect startup log — confirm D3D11 debug layer reports zero live-object warnings on shutdown (confirms TextureManager reset before Renderer); expected: clean shutdown with no "ID3D11ShaderResourceView leaked" messages
- [X] T021 [P] Verify no remaining `ResourceSystem` or `GetResourceSystem` references in LongXi/LXEngine/Src/Application/Application.cpp and LongXi/LXShell/Src/main.cpp; verify LXEngine.h includes TextureManager.h and Application.h; verify Texture/ files are referenced in the VS project (LXCore.vcxproj / LXEngine.vcxproj) if auto-registration occurred

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — **BLOCKS all user stories**
- **US1 (Phase 3)**: Depends on Phase 2 completion — largest phase, builds entire texture system
- **US2 (Phase 4)**: Depends on US1 completion — verifies cache behavior already in US1 code
- **US3 (Phase 5)**: Depends on US1 completion — verifies error paths already in US1 code; parallel with US2
- **Polish (Phase 6)**: Depends on all user story phases

### Within Phase 3 (US1)

```
T002 [P] TextureFormat.h  ┐
T003 [P] TextureData.h    ┘  (parallel, foundational)
         │
         ├─ T004 [P] TextureLoader.h (depends T003 for TextureData type)
         └─ T005 [P] DX11Renderer.h  (depends T002 for TextureFormat type)
                  │                          │
                  │                          ├─ T007 DX11Renderer.cpp
                  │                          └─ T008 Texture.h/.cpp
                  │
                  T006 TextureLoader.cpp (depends T004)
                  │
         (T006 + T007 + T008 all complete)
                  │
         T009 TextureManager.h
                  │
         T010 TextureManager.cpp
                  │
         T011 [P] Application.h  ┐
         T012 [P] LXEngine.h     ┘  (parallel, different files)
                  │
         T013 Application.cpp
```

### User Story Dependencies

- **US1 (P1)**: Depends on Foundational (Phase 2) — builds entire texture pipeline
- **US2 (P2)**: Depends on US1 — verifies cache normalization and hit path
- **US3 (P3)**: Depends on US1 — verifies all error return paths; can run in parallel with US2

---

## Parallel Execution Examples

### Phase 2: Foundational

```
Parallel: T002 — Create TextureFormat.h
Parallel: T003 — Create TextureData.h
```

### Phase 3: US1 (Two parallel groups after foundational)

```
Group A (after T002 + T003):
  Parallel: T004 — TextureLoader.h
  Parallel: T005 — DX11Renderer.h additions

Group B (after Group A):
  Parallel: T006 — TextureLoader.cpp    (depends T004)
  Parallel: T007 — DX11Renderer.cpp    (depends T005)
  Parallel: T008 — Texture.h/.cpp      (depends T005)

Then sequential: T009 → T010 (TextureManager)

Group C (after T010):
  Parallel: T011 — Application.h
  Parallel: T012 — LXEngine.h
  Sequential: T013 — Application.cpp (after T011)
```

### Phase 6: Polish

```
Parallel: T019 — Format all files
Parallel: T021 — Verify project references
Sequential: T018 — Build (after all source changes)
Sequential: T020 — Startup/shutdown verification (after T018)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete **Phase 1**: Setup (T001)
2. Complete **Phase 2**: Foundational (T002–T003)
3. Complete **Phase 3**: US1 (T004–T013) — full texture pipeline
4. **STOP and VALIDATE**: Application starts, `LoadTexture` returns valid textures, log shows `[Texture] Loaded:`
5. Polish (T018–T021) to confirm clean build

### Incremental Delivery

1. Phase 1 + 2 → Headers defined; no functional change yet
2. Phase 3 (US1) → Full pipeline working; Application exposes TextureManager
3. Phase 4 (US2) → Cache correctness confirmed
4. Phase 5 (US3) → Error safety confirmed
5. Phase 6 → Build clean, formatted, no live-object warnings

---

## Notes

- `[P]` = different files, no blocking dependencies — parallelizable
- `[US1/US2/US3]` maps to user stories in spec.md
- US2 and US3 add no new classes — they verify behavioral properties of the US1 implementation
- **Critical shutdown order** (T013): `m_TextureManager.reset()` MUST be the FIRST reset in `Application::Shutdown()` — ComPtr SRVs must be released while the DX11 device is still alive
- **Critical row pitch** (T007): DXT row pitch formula is `max(1,(W+3)/4) × blockSize`, NOT `W × blockSize` — wrong formula produces corrupt textures silently
- **Critical TGA row flip** (T006): Standard TGA is bottom-left origin; DirectX expects top-left — flip rows or textures appear upside-down
- All new C++ code must be inside `namespace LongXi { }` per CLAUDE.md Section 22
- Run `Win-Format Code.bat` before any commit — mandatory per CLAUDE.md
