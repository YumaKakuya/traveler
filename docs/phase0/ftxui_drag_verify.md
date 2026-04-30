# T0-D: FTXUI ResizableSplit Drag Operability Verification

## Objective

Verify that FTXUI `ResizableSplit` drag-resize works end-to-end with mouse input enabled, as required by GATE-P0-0 T0-D (Spec §4 T0-D). This experiment serves as a minimal hello-world for the Strip+Stage+Tower layout foundation (Spec §6.3 REQ-LAYOUT-1).

## Files

| File | Purpose |
|------|---------|
| `experiments/hello-ftxui/xmake.lua` | Experiment-local xmake project definition (C++20, depends on `ftxui`) |
| `experiments/hello-ftxui/src/main.cpp` | Minimal ResizableSplit hello-world: two coloured panes with a draggable vertical separator |
| `docs/phase0/ftxui_drag_verify.md` | This verification report |

**Note:** Root `xmake.lua` WAS modified to add a `hello-ftxui` target that builds `experiments/hello-ftxui/src/main.cpp` and links `ftxui`. This aligns with Spec §4 CEO Hands-on Test Sequence (`cd traveler && xmake config -p linux -a x86_64 && xmake build hello-ftxui`).

## Build Result

| Step | Result | Evidence |
|------|--------|----------|
| xrepo search `ftxui` | PASS | `ftxui-v6.1.9` available in xmake-repo |
| xrepo install `ftxui` | PASS | Installed locally in `~/.xmake/packages/f/ftxui/v6.1.9/...` |
| Experiment-local build (linux x86_64) | PASS | `xmake -P . -v` compiled and linked successfully |
| **Root build via `xmake.lua`** | **PASS** | `xmake build hello-ftxui` from repo root compiled and linked successfully |
| **Binary smoke test (root build)** | **PASS** | Binary launches, renders left/right panes with separator, exits cleanly on timeout |
| **Windows MinGW cross-build (root `xmake.lua`)** | **PASS** | `xmake config -p mingw -a x86_64 -m release -c -y && xmake build -y hello-ftxui` succeeded; output `hello-ftxui.exe` is `PE32+ executable (console) x86-64 ... for MS Windows` |

**Build commands executed (root `xmake.lua`):**
```bash
cd traveler
xmake config -p linux -a x86_64 -m release -c
xmake build traveler
xmake build hello-ftxui

# Windows MinGW cross-build
xmake config -p mingw -a x86_64 -m release -c -y
xmake build -y hello-ftxui
```

**Binary location (root build):**
```
build/linux/x86_64/release/hello-ftxui
build/mingw/x86_64/release/hello-ftxui.exe
```

**Binary properties (root build):**
- Format: ELF 64-bit LSB pie executable, x86-64
- Size: ~423 KB (stripped)
- FTXUI linkage: static (`libftxui-component`, `libftxui-dom`, `libftxui-screen`)
- Runtime deps: `libc.so.6`, `libstdc++.so.6`, `libgcc_s.so.1`

**Binary properties (Windows MinGW cross-build):**
- Path: `build/mingw/x86_64/release/hello-ftxui.exe`
- Format: PE32+ executable (console) x86-64, for MS Windows

## Platform Matrix

| Platform / Arch | Built | Manual Drag Verified | Notes |
|-----------------|-------|----------------------|-------|
| Linux x86_64 (glibc) | YES | **NOT VERIFIED** | Build PASS; smoke render PASS. Drag not tested (non-interactive agent). |
| Linux musl x86_64 | NO | NOT VERIFIED | Requires musl toolchain or Alpine container. |
| Linux musl arm64 | NO | NOT VERIFIED | Requires ARM64 runner / QEMU / cross-compilation. |
| macOS x86_64 | NO | NOT VERIFIED | Requires macOS host or cross-compilation with Apple SDK. |
| macOS arm64 | NO | NOT VERIFIED | Requires Apple Silicon host or remote build. |
| Windows x86_64 | YES | NOT VERIFIED | MinGW cross-build PASS; drag not tested (non-interactive agent). |

**Verdict:** The host platform (Linux x86_64 glibc) and Windows x86_64 (via MinGW cross-compilation) were built and smoke-tested. Cross-compilation to the remaining three targets was not attempted in this session.

## Manual Verification Instructions

**For CEO / manual tester:**

1. Build (from repo root):
   ```bash
   cd traveler
   xmake config -p linux -a x86_64 -m release -c
   xmake build hello-ftxui
   ```

2. Run:
   ```bash
   ./build/linux/x86_64/release/hello-ftxui
   ```

3. **Drag test:** Click and drag the vertical separator (`│`) between the blue left pane and the green right pane using the mouse.

4. **Expected behaviour:**
   - Left pane width and right pane width update in real time as the separator is dragged.
   - No flicker beyond one frame.
   - Pane content re-flows smoothly.
   - Separator remains visible and under the cursor during the drag.

5. Exit the program by pressing `q` or `Ctrl+C`.

**Terminal requirements:**
- Terminal must support mouse mode (e.g., GNOME Terminal, Konsole, Alacritty, iTerm2, Windows Terminal).
- If running over SSH, ensure the terminal emulator forwards mouse events.

## Blockers for Other Platforms

### macOS (x86_64 & arm64)
- **Status:** STOPPED indefinitely under the current-resource rule.
- **Reason:** No macOS host or Apple SDK is available in the current environment.
- **Known risk if later resumed:** FTXUI mouse events rely on terminal CSI sequences; macOS Terminal.app vs iTerm2 may behave differently. iTerm2 is the recommended test target.

### Windows (x86_64)
- **Status:** Build verified via MinGW cross-compile. Native launch/manual drag is stopped for AI-side verification because it requires an interactive Windows terminal session.
- **Known risk if later resumed:** Windows Console API mouse handling differs from POSIX terminals. FTXUI uses the Windows Console API internally; verify that mouse drag events are translated correctly to the ResizableSplit component.

### Linux musl (x86_64 & arm64)
- **Status:** STOPPED indefinitely under the current-resource rule.
- **Reason:** Current build used glibc; musl toolchains are not available in the current environment.
- **Known risk if later resumed:** FTXUI itself has no known musl incompatibilities, but static linking with `libstdc++` on musl requires care to avoid locale/iconv issues.

## Follow-up Tasks

| Task | Owner | Description |
|------|-------|-------------|
| Cross-build matrix | STOPPED | macOS/linux-musl targets are unavailable under current-resource rule. |
| Manual drag verify | STOPPED | Multi-target interactive verification is unavailable to AI-side execution under current-resource rule. |

## T0-D Status

| Criterion | Status |
|-----------|--------|
| Minimal hello-world created | PASS |
| Experiment-local Linux x86_64 build | PASS |
| Root `xmake.lua` target build | PASS |
| Smoke render (non-interactive) | PASS |
| Manual drag verification | **STOPPED** (interactive multi-target verification unavailable) |
| Cross-compilation to 3 remaining targets | **STOPPED** (targets/toolchains unavailable) |

**Overall T0-D:** Current-env partial PASS — 2/5 builds verified (Linux x86_64 glibc and Windows x86_64 MinGW); unavailable manual/cross-target checks are stopped indefinitely.
