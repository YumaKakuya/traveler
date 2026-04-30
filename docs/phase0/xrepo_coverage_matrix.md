# xrepo Package Resolution Coverage Matrix
# GATE-P0-0 T0-A
# -------------------------------------------------------
# Generated: 2026-04-30
# Host: WSL2 Ubuntu x86_64 (glibc 2.39)
# xmake: v3.0.8+20260430
# xrepo: v3.0.8+20260430
# Authority: Traveler_Phase0_Spec_v0.1.md §4 T0-A
# -------------------------------------------------------

## Summary Verdict

| Metric | Result | Spec Threshold |
|--------|--------|----------------|
| xrepo-resolvable dependencies | 9 / 10 | >= 9 / 10 |
| Submodule fallback count | 2 | <= 2 |
| Five-target build verified | NOT VERIFIED | N/A (host limitation) |
| Linux musl static verified | NOT VERIFIED | N/A (host limitation) |

**T0-A Verdict: CONDITIONAL PASS — xrepo resolution meets thresholds, but multi-target static-link verification is blocked by missing cross-compilation toolchains on this host.**

---

## Command Log Summary

Commands executed on host `DESKTOP-13M8JNI` (WSL2 Ubuntu, x86_64, glibc 2.39):

```bash
# Tool availability
$ xmake --version                  # v3.0.8+20260430
$ xrepo --version                  # v3.0.8+20260430

# Package searches (one per dependency)
$ xrepo search ftxui               # -> ftxui-v6.1.9
$ xrepo search notcurses           # -> (no results)
$ xrepo search llama               # -> llama.cpp-3775
$ xrepo search tree-sitter         # -> tree-sitter-v0.26.8
$ xrepo search sqlite3             # -> sqlite3-3.53.0+0
$ xrepo search libcurl             # -> libcurl-8.11.0
$ xrepo search openssl             # -> openssl3-3.6.2
$ xrepo search nlohmann_json       # -> nlohmann_json-v3.12.0
$ xrepo search toml++              # -> toml++-v3.4.0
$ xrepo search spdlog              # -> spdlog-v1.17.0

# Package detail info
$ xrepo info ftxui                 # platforms: linux, macosx, windows, mingw, bsd, cross
$ xrepo info llama.cpp             # platforms: all
$ xrepo info tree-sitter           # platforms: all
$ xrepo info sqlite3               # platforms: all
$ xrepo info libcurl               # platforms: linux, android, mingw, macosx, wasm, bsd, windows, cross, iphoneos
$ xrepo info openssl3              # platforms: cross, linux, bsd, mingw, wasm, msys, iphoneos, macosx, windows, android
$ xrepo info nlohmann_json         # platforms: all
$ xrepo info toml++                # platforms: all
$ xrepo info spdlog                # platforms: all

# Physical install + static-object verification (host: linux x86_64 glibc)
$ xrepo install --yes ftxui        # -> libftxui-component.a, libftxui-dom.a, libftxui-screen.a
$ xrepo install --yes tree-sitter  # -> libtree-sitter.a
$ xrepo install --yes toml++       # -> header-only (confirmed installed)
$ xrepo install --yes spdlog       # -> header-only (confirmed installed)
# sqlite3, libcurl, openssl3, llama.cpp were already cached; static objects confirmed via `find`.
```

---

## Dependency Matrix

| # | Dependency | xmake Package Name | Version | License | Platforms (xrepo claim) | Static on Host | Five-Target Verified |
|---|------------|-------------------|---------|---------|------------------------|----------------|----------------------|
| 1 | FTXUI | `ftxui` | v6.1.9 | MIT | linux, macosx, windows, mingw, bsd, cross | YES (.a produced) | NOT VERIFIED |
| 2 | notcurses-considered | — | — | — | **NOT IN XREPO** | N/A | N/A |
| 3 | llama.cpp | `llama.cpp` | 3775 | MIT | all | YES (.a produced) | NOT VERIFIED |
| 4 | Tree-sitter (core) | `tree-sitter` | v0.26.8 | MIT | all | YES (.a produced) | NOT VERIFIED |
| 5 | SQLite | `sqlite3` | 3.53.0+0 | Public Domain | all | YES (.a produced) | NOT VERIFIED |
| 6 | libcurl | `libcurl` | 8.11.0 | MIT | linux, android, mingw, macosx, wasm, bsd, windows, cross, iphoneos | YES (.a produced) | NOT VERIFIED |
| 7 | OpenSSL | `openssl3` | 3.6.2 | Apache-2.0 | cross, linux, bsd, mingw, wasm, msys, iphoneos, macosx, windows, android | YES (.a produced) | NOT VERIFIED |
| 8 | nlohmann/json | `nlohmann_json` | v3.12.0 | MIT | all | header-only | NOT VERIFIED |
| 9 | toml++ | `toml++` | v3.4.0 | MIT | all | header-only | NOT VERIFIED |
| 10 | spdlog | `spdlog` | v1.17.0 | MIT | all | header-only | NOT VERIFIED |

**Notes on columns:**
- *Static on Host*: Verified by `xrepo install` on WSL2 Ubuntu x86_64 (glibc). `.a` files observed for compiled libraries; header-only status confirmed for the three header-only packages.
- *Five-Target Verified*: NOT VERIFIED for all entries because the host lacks toolchains for linux-musl, arm64, and macOS (see Open Blockers).

---

## Five-Target Coverage Matrix (Spec §4 T0-A Requirement)

| Dependency | linux-musl-x86_64 | linux-musl-arm64 | macos-x86_64 | macos-arm64 | windows-x86_64 |
|------------|-------------------|------------------|--------------|-------------|----------------|
| FTXUI | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |
| notcurses-considered | SUBMODULE FALLBACK | SUBMODULE FALLBACK | SUBMODULE FALLBACK | SUBMODULE FALLBACK | SUBMODULE FALLBACK |
| llama.cpp | SUBMODULE FALLBACK | SUBMODULE FALLBACK | SUBMODULE FALLBACK | SUBMODULE FALLBACK | SUBMODULE FALLBACK |
| Tree-sitter (core) | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |
| SQLite | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |
| libcurl | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |
| OpenSSL | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |
| nlohmann/json | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |
| toml++ | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |
| spdlog | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS | XREPO CLAIM PASS |

**Legend:**
- `HOST VERIFIED` — Physically verified on this host (Linux x86_64 glibc). None of the five targets above are at this status because the required target triples differ from the host.
- `XREPO CLAIM PASS` — xrepo package metadata claims platform support; no physical build attempted on this host due to missing toolchain.
- `SUBMODULE FALLBACK` — Dependency is intentionally resolved via git submodule + static link per Architecture Decision A-4 (llama.cpp) or because the package is not in xrepo (notcurses).
- `NOT VERIFIED` — No xrepo claim and no fallback path exercised.

**Physical verification summary:**
| Target | Status |
|--------|--------|
| linux-musl-x86_64 | NOT VERIFIED (no musl toolchain) |
| linux-musl-arm64 | NOT VERIFIED (no arm64 cross compiler) |
| macos-x86_64 | NOT VERIFIED (no macOS SDK) |
| macos-arm64 | NOT VERIFIED (no macOS SDK) |
| windows-x86_64 | PARTIAL VERIFIED — `hello-ftxui` (FTXUI only) built via MinGW cross-compile; PE32+ executable produced. Full dependency matrix not verified. |

**PC-1 completeness:** The xrepo resolution threshold (≥ 9/10) is met and submodule fallback count (2) is within threshold. However, **physical five-target verification is missing**, so PC-1 cannot be claimed as fully closed until cross-compilation toolchains are available and at least one dependency per target is built.

---

## Submodule / Fallback Candidates

| Dependency | Fallback Reason | Fallback Method | Spec Reference |
|------------|-----------------|-----------------|----------------|
| llama.cpp | Design decision (A-4) | Git submodule `third_party/llama.cpp/` + static link | Proposal §4.6 / §9.4 T-8 |
| notcurses-considered | **xrepo unresolved** | Git submodule `third_party/notcurses/` + manual build script, OR drop from Phase 0 if FTXUI primary suffices | Proposal §4.2 secondary |

**Submodule-fallback count = 2** (within Spec threshold of <= 2).

### Additional Out-of-Scope Finding

Tree-sitter **language grammars** (C, C++, Python, JavaScript, TypeScript, Rust, Go, Java, Ruby, Bash) are **not individually packaged in xrepo**. Only the core `tree-sitter` library is available. Grammars will require:
- Git submodules under `third_party/tree-sitter-grammars/` (or similar), OR
- Direct source vendoring at build time.

This does **not** count against the T0-A 10-dependency limit because the Spec T0-A dependency list refers to "Tree-sitter" (the core library). Grammar resolution is an RD-05 / GATE-P0-2 concern.

---

## Open Blockers

| # | Blocker | Impact | Mitigation / Next Step |
|---|---------|--------|------------------------|
| B-1 | **No musl toolchain** on host (`x86_64-linux-musl-gcc` missing) | Cannot verify linux-musl-x86_64 static linkage | STOPPED indefinitely under current-resource rule. |
| B-2 | **No arm64 cross compiler** on host (`aarch64-linux-gnu-gcc` missing) | Cannot verify linux-musl-arm64 static linkage | STOPPED indefinitely under current-resource rule. |
| B-3 | **No macOS SDK / toolchain** on host | Cannot verify macos-x86_64 or macos-arm64 builds | STOPPED indefinitely under current-resource rule. |
| B-4 | **Windows cross-compilation partially exercised** | `x86_64-w64-mingw32-gcc` present; `hello-ftxui` (FTXUI only) built successfully via `xmake config -p mingw -a x86_64 -m release -c -y && xmake build -y hello-ftxui`. Full dependency matrix and static-CRT path not yet verified. | DEFERRED until later packaging work requires full static-CRT proof. |
| B-5 | **notcurses not in xrepo** | Secondary TUI framework unavailable via xrepo | STOPPED as Phase 0 fallback because FTXUI current-env build passed. |

---

## Next Verification Commands

Stopped indefinitely under the current-resource rule. Do not execute additional cross-target verification unless the required toolchain/host already exists in the current environment.

Historical commands kept below for reference only:

```bash
# 1. musl static verification (linux-musl-x86_64)
#    Install musl toolchain, then:
xrepo install --yes --toolchain=musl --arch=x86_64 ftxui
xrepo install --yes --toolchain=musl --arch=x86_64 sqlite3
#    Verify .a files are produced and linkable.

# 2. Windows static-CRT verification
xmake f -p mingw -a x86_64 -c
xmake b
#    With a minimal target that includes `add_requires("ftxui", "sqlite3")`
#    and `set_runtimes("MT")`.

# 3. arm64 cross-compilation verification (GitHub Actions)
#    Add a matrix cell to CI:
#    - os: ubuntu-latest
#    - arch: arm64
#    - toolchain: aarch64-linux-musl-
#    Run `xmake f -p linux -a arm64 --toolchain=musl ... && xmake b`

# 4. macOS verification (GitHub Actions or physical mac)
#    Run on macos-latest GA runner:
#    xmake f -p macosx -a x86_64 -c && xmake b
#    xmake f -p macosx -a arm64 -c && xmake b
```

---

## License Compatibility Note

All xrepo-resolved dependencies carry AGPL-compatible licenses:

| Package | License | AGPL-Compatible |
|---------|---------|-----------------|
| ftxui | MIT | YES |
| llama.cpp | MIT | YES |
| tree-sitter | MIT | YES |
| sqlite3 | Public Domain | YES |
| libcurl | MIT | YES |
| openssl3 | Apache-2.0 | YES |
| nlohmann_json | MIT | YES |
| toml++ | MIT | YES |
| spdlog | MIT | YES |

No license-audit red flags for the xrepo-resolved set.

---

## Sign-Off

- **Scope**: GATE-P0-0 T0-A only.
- **Head at generation**: `7778a0afb3a63936d3a95c3c43d2f3b493760d81`
- **Worktree state**: clean except for untracked `docs/` and `experiments/`
- **Physical verification performed**: xrepo search + info + install on linux x86_64 glibc host; static objects confirmed for compiled libs; header-only status confirmed for three libs.
- **Physical verification NOT performed**: cross-compilation to linux-musl, linux-arm64, macOS, Windows; static-link verification on non-host targets.
