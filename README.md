# Traveler.

**Work in Progress -- Phase 0**

This project is in active development during the Phase 0 sprint (v0.1.0). API, commands, and behaviour may change.

---

Traveler. brings AI coding to the next billion developers -- single binary, offline-capable, runs on a 2011 Core i3 HDD with 4GB RAM

Traveler. is a sister product to Hatch. Gen 1, which continues in maintenance as a separate product.

---

## Table of Contents

- [Overview](#overview)
- [Install](#install)
- [Modes: Cockpit vs LLM](#modes-cockpit-vs-llm)
- [Provider Setup](#provider-setup)
- [Phase 0 Scope](#phase-0-scope)
- [License](#license)

---

## Overview

Traveler. is a single-binary, offline-capable, native C++ TUI coding agent built for hardware that modern AI tools have abandoned. It integrates:

- **Helix DNA** -- modal-optional editor, Tree-sitter syntax, `space` leader cheatsheet
- **Zellij DNA** -- multi-pane workspace, layouts, session management
- **lazygit DNA** -- git workflow TUI
- **Cockpit** -- Window-Manager-native multi-callsign workspace (up to 4 simultaneous mounts in cloud mode; 1 in offline mode)
- **Slash + leader + @ + Command Palette dispatcher**
- **llama.cpp inference** -- embedded offline mode via "Ayane" model

**Core i3 HDD target is the design aspiration; first-release verified hardware floor will be confirmed at v0.1.0 post-release.**

### License

Traveler. is licensed under **AGPL v3** with a mandatory Contributor License Agreement (CLA).  
Copyright assigned to **Sorted.**

For details, see [LICENSE](./LICENSE) and [CLA.md](./CLA.md).

---

## Install

Install instructions for v0.1.0 and later will be published at release.

### Build from source (prerequisites: C++20, xmake)

```bash
git clone https://github.com/yumakakuya/traveler.git
cd traveler
xmake config -m release
xmake build
./build/linux/x86_64/release/traveler --version
```

Pre-built binaries for linux-musl-x86_64, linux-musl-arm64, macos-x86_64, macos-arm64, and windows-x86_64 will be published with each GitHub release.

---

## Modes: Cockpit vs LLM

Traveler. ships two primary interaction modes:

### Cockpit (default, power-user surface)

Cockpit is the default mode at launch. It provides a multi-callsign workspace powered by the 9-pillar Window Manager surface:

- Up to 4 simultaneous callsign mounts (cloud mode) or 1 mount (offline mode)
- Strip + Stage + Tower layout
- Focused callsign populates the Stage; non-focused callsigns hold frozen snapshots in the Strip
- `space` leader cheatsheet, `@` callsign mention, `/` slash commands

### LLM Mode (simple single-session chat)

LLM mode is a lower-complexity path -- a simple single-session chat interface:

- One callsign, one session
- No multi-pane Strip/Stage/Tower layout
- Straightforward prompt input

Switch between modes via `/Cockpit`, `/LLM`, `/Editor`, `/Pane`, `/Git`, or `/Split`.

---

## Provider Setup

Traveler. supports multiple LLM provider paths. Configure via environment variables or `traveler providers login`.

### Anthropic

**OAuth (browser-based, recommended for desktop users):**

```bash
traveler providers login anthropic --oauth
```

**API key (recommended for users on intermittent / mobile-tier networks):**

```bash
export ANTHROPIC_API_KEY=your-api-key
# or
traveler providers login anthropic --api-key
```

### OpenAI

```bash
export OPENAI_API_KEY=your-api-key
# or
traveler providers login openai --api-key
```

### Google

```bash
export GOOGLE_API_KEY=your-api-key
# or
traveler providers login google --api-key
```

### Offline (Ayane -- no network after first-run model download)

Traveler. bundles an embedded llama.cpp inference engine running the **"Ayane"** model:

- No network required after first-run model download
- Restricted to 1 callsign mount (Core-i3-class hardware budget)
- First-run prompts for model download; thereafter fully offline

---

## Phase 0 Scope

### Included in Phase 0 / v0.1.0

- Helix DNA: modal-optional editor, Tree-sitter syntax, `space` leader, file picker
- Zellij DNA: multi-pane workspace (base resize)
- lazygit DNA: git workflow TUI (keybind discoverability)
- Cockpit 9-pillar: multi-callsign workspace, 4-mount cloud / 1-mount offline
- Command Palette + Slash + leader + @ dispatcher
- llama.cpp offline inference ("Ayane")
- OAuth + API-key authentication paths for Anthropic, OpenAI, Google
- AGPL v3 + mandatory CLA

### Not in v0.1.0 (deferred to Phase 1+)

| Feature | Reason |
|---------|--------|
| LSP integration | Tree-sitter covers syntax; LSP deferred |
| Pane drag-resize polish (layouts, swap, resurrection) | Base resize only in Phase 0 |
| Git rebase / cherry-pick / interactive rebase | Deferred to Phase 1+ |
| Discord / Newsletter | Not adopted day-0; GitHub + X only |

---

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](./CONTRIBUTING.md) for the contribution policy, CLA signing instructions, and information-hygiene rules.

Please also review our [Code of Conduct](./CODE_OF_CONDUCT.md).
