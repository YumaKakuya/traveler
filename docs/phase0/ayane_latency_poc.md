# GATE-P0-0 T0-C: V-2 "Ayane" Model First-Token Latency PoC

**Status:** BUILD PASS / LATENCY STOPPED  
**Date:** 2026-04-30  
**Assigned:** Senior (T0-C)  
**Spec Reference:** Traveler_Phase0_Spec_v0.1.md §4 T0-C

---

## 1. Objective

Determine whether the two CEO-mandated offline models — **phi-3.5-mini** and **llama-3.2-1b-class** (both Q4_K_M quantized) — achieve first-token latency < 3 seconds on Core-i3-class CPU when invoked via llama.cpp.

Pass criterion (Spec PC-3): both models median < 3 s over 10 trials.

---

## 2. Hardware Observed

| Attribute | Value |
|-----------|-------|
| CPU | 11th Gen Intel(R) Core(TM) i5-1145G7 @ 2.60GHz |
| Cores / Threads | 4 / 8 |
| Base / Turbo | 2.60 GHz / ~4.40 GHz |
| RAM | 7.6 GiB total (3.3 GiB available at test time) |
| Disk (project FS) | 883 GiB available on /dev/sdc |
| OS | Linux (WSL / native container) |

**CPU-class note:** The host is an i5-1145G7 (Tiger Lake, 2021), not a 2011 Core-i3. It is approximately 2–3× faster in single-thread performance than a Sandy Bridge Core-i3 (e.g., i3-2100). Therefore, latencies measured on this machine would need a **translation factor** (~1.5×–2.5×) to estimate true Core-i3-class performance. The Spec allows proxy hardware with documented translation factor (T0-C Method step 3). A true Core-i3-class machine is required for final v0.1.0 verification.

---

## 3. Tool Availability

| Tool | Status | Path / Version |
|------|--------|----------------|
| xmake | Available | v3.0.8+20260430 |
| g++ | Available | /usr/bin/g++ |
| make | Available | /usr/bin/make |
| git | Available | /usr/bin/git |
| cmake | Not on PATH | `/usr/bin/cmake` not found; xrepo cmake 4.2.3 cached at `~/.xmake/packages/c/cmake/4.2.3/.../bin/cmake` |
| llama.cpp submodule | **MISSING** | `third_party/llama.cpp` does not exist; `git submodule status` empty |
| llama-cli / llama-server | **MISSING** | not installed |
| xrepo llama.cpp package | **Installed** | `llama.cpp 3775` installed under `~/.xmake/packages/l/llama.cpp/3775/...` with `libllama.a`, `libggml.a`, `include/llama.h` |

**Conclusion:** llama.cpp is available as an xrepo package and the harness compiles and links successfully. The git submodule is still absent (Spec A-4) but is not required for the local experiment build.

---

## 4. Model URL Candidates

The following are safely identifiable public Hugging Face official endpoints. Sizes are approximate and were verified via Hugging Face web metadata (not downloaded).

| Model | Quantization | Approx. Size | Candidate URL (Hugging Face) |
|-------|--------------|--------------|------------------------------|
| **Phi-3.5-mini-instruct** | Q4_K_M | ~2.2 GiB | `https://huggingface.co/bartowski/Phi-3.5-mini-instruct-GGUF` (or `microsoft/Phi-3.5-mini-instruct` official GGUF releases) |
| **Llama-3.2-1B-Instruct** | Q4_K_M | ~0.7 GiB | `https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF` (or `meta-llama/Llama-3.2-1B-Instruct` official GGUF releases) |

**Size verification:**  
- phi-3.5-mini Q4_K_M ≈ 2.2 GB  
- llama-3.2-1b Q4_K_M ≈ 0.7 GB  
- Combined download: ~2.9 GB  
- Disk feasibility: **PASS** (883 GB available).  
- RAM feasibility: **MARGINAL** (7.6 GB total; llama.cpp CPU inference for 2.2 GB model plus overhead fits within available 3.3 GB, but swap may be exercised. A true 4 GB Core-i3 machine would be at risk.)

**Network:** Hugging Face (`https://huggingface.co`) is reachable (HTTP 200 confirmed via `curl -I`). No firewall block observed.

---

## 5. Exact Blockers

T0-C cannot proceed to execution because of the following blockers, listed in resolution order:

### Blocker B1 — cmake missing (RESOLVED for build)
- **Evidence:** `which cmake` returns empty, but xrepo has cmake 4.2.3 cached.
- **Impact:** llama.cpp from source would need cmake, but the pre-built xrepo package `llama.cpp 3775` is already installed.
- **Resolution:** Not blocking the harness build. System cmake installation remains optional.

### Blocker B2 — llama.cpp dependency not provisioned (RESOLVED for experiment build)
- **Evidence:** `third_party/llama.cpp` directory does not exist; `git submodule status` returns empty.
- **Impact:** The root project does not yet have the llama.cpp submodule (Spec A-4).
- **Resolution:** The local `experiments/ayane-latency/xmake.lua` uses `add_requires("llama.cpp 3775")` to consume the xrepo-installed package. The submodule must still be added for the main project build (out of T0-C scope).

### Blocker B3 — no GGUF model files on host (CRITICAL)
- **Evidence:** `find /home/yuma/traveler -name "*.gguf"` returns empty.
- **Impact:** No model to load for inference. The harness binary exits with a clear error when the model path does not exist.
- **Resolution:** STOPPED indefinitely under the current-resource rule. No model downloads are requested.

### Blocker B4 — non-representative CPU class (WARNING)
- **Evidence:** Host CPU is i5-1145G7 (2021), not a 2011 Core-i3.
- **Impact:** Latencies measured here will be optimistic relative to the target hardware class.
- **Resolution:** STOPPED indefinitely for GATE-P0-0. Do not request target hardware or proxy designation.

---

## 6. Harness

A buildable PoC harness is maintained under `experiments/ayane-latency/`:

| File | Purpose |
|------|---------|
| `experiments/ayane-latency/main.cpp` | C++20 harness using llama.cpp C API with `std::chrono` timing capture, CLI parsing, and KV-cache reset between runs |
| `experiments/ayane-latency/xmake.lua` | Local build definition using `add_requires("llama.cpp 3775")` |
| `experiments/ayane-latency/README.md` | Invocation instructions and timing methodology |

The harness does **not** bundle model files and uses a command-line path argument, satisfying the git-size constraint.

---

## 7. Current-Resource Commands

The harness is buildable. Latency execution is stopped indefinitely because no GGUF model files exist locally.

```bash
# Build harness (already done; rebuild if source changes)
cd /home/yuma/traveler/experiments/ayane-latency
xmake -P . -v

# Confirm current-resource stop condition
./build/linux/x86_64/release/ayane-latency /nonexistent/model.gguf
```

---

## 8. Latency Table (Stopped)

| Model | Hardware | Runs | Min (ms) | Median (ms) | Max (ms) | Pass (< 3 s) |
|-------|----------|------|----------|-------------|----------|--------------|
| phi-3.5-mini Q4_K_M | stopped: no local GGUF | — | — | — | — | — |
| llama-3.2-1b Q4_K_M | stopped: no local GGUF | — | — | — | — | — |

---

## 9. Escalation Path

Per CEO current-resource clarification, no R-T06 choice is requested while model files and target hardware are absent.

- Latency execution is stopped indefinitely until GGUF files already exist locally.
- Core-i3-class verification is stopped indefinitely until such hardware already exists locally.

---

## 10. Self-Check Summary

| Check | Result |
|-------|--------|
| Harness created | Yes (`experiments/ayane-latency/*`) |
| No model files in git | Yes (external path argument) |
| Hardware documented | Yes (i5-1145G7, proxy noted) |
| Tool availability documented | Yes (cmake missing on PATH, but xrepo cache unblocks build) |
| Model URLs identified | Yes (Hugging Face official endpoints) |
| Exact blockers listed | Yes (B1 resolved, B2 resolved for experiment, B3 critical, B4 warning) |
| Next commands documented | Yes (§7) |
| Build attempted | **Yes — PASS** (`xmake -P . -v` from `experiments/ayane-latency/` builds and links successfully) |
| Binary runs without model | Yes — prints clear error when model file is missing |
| Status | **BUILD PASS / LATENCY STOPPED** |

---

*Document generated by Senior agent for GATE-P0-0 T0-C.*
*HEAD: 7778a0afb3a63936d3a95c3c43d2f3b493760d81*
