# Ayane Latency PoC Harness

## Purpose

Minimal C++ harness for measuring first-token latency of quantized LLM models via llama.cpp, as required by GATE-P0-0 T0-C (Spec §4 T0-C).

## Build

```bash
cd experiments/ayane-latency
xmake -P . -v
```

The harness links against the xrepo-provided `llama.cpp 3775` package. No local git submodule or system cmake is required.

## Run

```bash
# Run with model path (GGUF)
./build/linux/x86_64/release/ayane-latency <path-to-model.gguf> [--runs 10] [--prompt "Hello, traveler."] [--threads 4]
```

## Timing Capture Method

1. `std::chrono::high_resolution_clock` is used for wall-clock measurement.
2. **T0** = timestamp immediately before `llama_decode()` / `llama_sampler_sample()` for the first token.
3. **T1** = timestamp immediately after the first token ID is emitted.
4. Latency = T1 - T0 (milliseconds).
5. The harness repeats the measurement for `--runs` trials, clearing the KV cache between each run.
6. Output format: `min / median / max` across all trials.

## Output Example (illustrative)

```
Ayane Latency PoC Harness
=========================
Model path: /path/to/phi-3.5-mini.Q4_K_M.gguf
Prompt:     "Hello, traveler."
Runs:       10

Run 1: 1234.56 ms
...

Latency Summary (ms)
  min    = 1200.12
  median = 1234.56
  max    = 1289.01
```

## Files

| File | Description |
|------|-------------|
| `main.cpp` | Harness source using llama.cpp C API |
| `xmake.lua` | Local build target (depends on llama.cpp package) |
| `README.md` | This file |

## Model Files

GGUF files are **NOT** committed to git. The harness expects an external path. For T0-C, the target models are:
- `phi-3.5-mini` Q4_K_M GGUF
- `llama-3.2-1b` Q4_K_M GGUF

## Status

See `docs/phase0/ayane_latency_poc.md` for current build/run status and blockers.
