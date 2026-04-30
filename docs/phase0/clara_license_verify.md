# T0-B: Clara License Compatibility with Traveler. AGPL v3 Inbound

**Date:** 2026-04-30
**Task:** GATE-P0-0 T0-B
**Author:** Traveler. Senior
**Verdict:** FALLBACK

---

## 1. Summary Verdict

**FALLBACK — clean-room reimplementation required.**

The Clara repository contains **no license file and no in-file license headers** for the four target modules. AGPL v3 compatibility cannot be established from verified repository evidence. Per Spec §4 T0-B Method step 3, the R-T04 risk has materialised: Clara license is unfinalised. CEO consultation is required to either (a) finalise Clara license to an AGPL-compatible license at the Clara repo level, or (b) execute clean-room reimplementation of the four modules inside Traveler.

---

## 2. Repositories Inspected

| Repository | Path | License File Found | Notes |
|---|---|---|---|
| Clara | `/home/yuma/Clara` | **None** | No `LICENSE`, `COPYING`, or `NOTICE` at root or any subdir |
| Arisa | `/home/yuma/arisa` | **None** | Arisa is a separate AXIOM. product; no Clara code present |

**Commands executed:**
- `find /home/yuma/Clara -maxdepth 3 -type f \( -iname '*license*' -o -iname '*copying*' -o -iname '*notice*' \)` → no matches
- `find /home/yuma/arisa -maxdepth 3 -type f \( -iname '*license*' -o -iname '*copying*' -o -iname '*notice*' \)` → no matches
- `git -C /home/yuma/Clara remote -v` → no remotes configured
- `git -C /home/yuma/Clara log --all --oneline -- '*LICENSE*' '*COPYING*' '*NOTICE*'` → no commits touching license files

---

## 3. License File Evidence

### 3.1 Clara Root
- **Result:** No `LICENSE`, `COPYING`, `NOTICE`, or equivalent file exists at `/home/yuma/Clara/` root or in any subdirectory.
- **SPDX Identifier:** None recorded.

### 3.2 Clara Docs
- **Result:** `/home/yuma/Clara/docs/` contains CTO session briefs and a Wizard session brief. No license grant or project-level SPDX declaration found.
- **Reference:** `docs/Wizard_Session_Brief_2026-04-30.md` mentions `~/Clara/research/license/_SUMMARY.md` as a legal premise, but that file covers **training data sources** (Tatoeba, WikiMatrix, etc.), not Clara source-code licensing.

### 3.3 Clara Research / License Directory
- **Path:** `/home/yuma/Clara/research/license/`
- **Content:** Extensive research on third-party **training data** licenses for NMT/TM corpora (CC-BY, CC-BY-SA, proprietary, etc.).
- **Relevance to T0-B:** **Zero**. These files discuss data-source licenses for machine-learning training sets, not the copyright license under which the Clara C++ implementation itself is distributed.

### 3.4 Arisa Prior Investigation
- **File:** `/home/yuma/arisa/docs/gate-p1-0/clara-license-status.md`
- **Conclusion:** Identical findings — no Clara license file or source-file header found. Status recorded as "UNCLEAR but CEO-declaration-governed."
- **Cross-reference:** This confirms the absence of license evidence is not an oversight; it is a known, pre-existing gap.

---

## 4. Per-Module Header Evidence

### 4.1 `llm.cpp` + `llm.h`
- **Paths:**
  - `/home/yuma/Clara/src/llm.cpp` (616 lines)
  - `/home/yuma/Clara/src/llm.h` (77 lines)
- **Header evidence:** No SPDX-License-Identifier, no Copyright line, no license grant block. First line is `#include "llm.h"` (cpp) or `#pragma once` (h).

### 4.2 `db.cpp` + `db.h`
- **Paths:**
  - `/home/yuma/Clara/src/db.cpp` (501 lines)
  - `/home/yuma/Clara/src/db.h` (58 lines)
- **Header evidence:** No SPDX-License-Identifier, no Copyright line, no license grant block. First line is `#include "db.h"` (cpp) or `#pragma once` (h).

### 4.3 `clipboard.cpp` + `clipboard.h`
- **Paths:**
  - `/home/yuma/Clara/src/clipboard.cpp` (96 lines)
  - `/home/yuma/Clara/src/clipboard.h` (14 lines)
- **Header evidence:** No SPDX-License-Identifier, no Copyright line, no license grant block. First line is `#include "clipboard.h"` (cpp) or `#pragma once` (h).

### 4.4 `sanitize.cpp` + `sanitize.h`
- **Paths:**
  - `/home/yuma/Clara/src/sanitize.cpp` (133 lines)
  - `/home/yuma/Clara/src/sanitize.h` (16 lines)
- **Header evidence:** No SPDX-License-Identifier, no Copyright line, no license grant block. First line is `#include "sanitize.h"` (cpp) or `#pragma once` (h).

### 4.5 grep confirmation
- **Command:** `grep -ri 'SPDX\|copyright\|licens\|licenced\|MIT\|AGPL\|GPL\|Apache\|BSD' /home/yuma/Clara/src/ /home/yuma/Clara/Makefile /home/yuma/Clara/Makefile.win32`
- **Result:** Zero matches for any license-related term in source files or build files. Only false positives on `limit` (C++ parameter name) and `commit` (git log term) in docs.

---

## 5. AGPL Compatibility Assessment

| Criterion | Assessment |
|---|---|
| Clara has a repo-level LICENSE file | **FAIL** — none found |
| Clara has in-file SPDX or license headers | **FAIL** — none found in any of the 8 inspected files (4 cpp + 4 h) |
| Clara license is OSI-approved and AGPL-compatible | **FAIL** — no license to evaluate |
| Traveler. can inline-copy under D-2 (AGPL v3) | **STOPPED** — inbound license is undefined; copying unlicensed code into AGPL v3 creates legal uncertainty |

**Legal reasoning (Spec-bound):**
- Traveler. is bound to **AGPL v3 + mandatory CLA** per Decision D-2 (Proposal §5.1 / §5.2).
- Inline-copying code with **no declared license** into an AGPL v3 project violates the "verify before assert" principle and exposes the project to copyright infringement risk.
- The Spec explicitly anticipates this scenario as **R-T04 risk materialisation** and mandates CEO consultation.

---

## 6. Fallback Path

### 6.1 Current-Resource Decision
CEO clarification (2026-04-30): unavailable or unverifiable items stop indefinitely. Therefore Clara inline-copy is stopped indefinitely; no CEO path selection is required for GATE-P0-0.

| Path | Status | Timeline Impact | GATE Impact |
|---|---|---|---|
| License finalisation | STOPPED for Traveler. planning; no license evidence exists now. | N/A | Do not inline-copy Clara source. |
| Clean-room reimplementation | Later implementation path if functionality is needed. | ~1 week (absorbed in Phase 0 buffer per Proposal §11.4 R-T04 mitigation) | T0-B PASS via fallback evidence. |

### 6.2 Recommended Fallback: Clean-Room Reimplementation
**Rationale:**
- Clara is a separate Sorted. product with its own roadmap (translation engine). Binding its license to Traveler.'s needs creates cross-product coupling.
- The four modules are small and functionally well-defined (~1,346 total lines of C++):
  - `clipboard`: platform abstraction (~96 lines)
  - `sanitize`: UTF-8 text cleaning (~133 lines)
  - `db`: SQLite3 FTS5 wrapper (~501 lines, but Traveler. needs a different schema)
  - `llm`: HTTP JSON API client (~616 lines, but Traveler. needs a provider-plugin abstraction per A-10)
- Clean-room reimplementation aligns with Spec §1.1 "reference-driven port over greenfield design" only for Hatch. Gen 1; Clara modules are **not** the Hatch. reference. Reimplementing them is within scope.

### 6.3 Clean-Room Reimplementation Plan (WB-02 Adjustment)

| Module | Traveler. Target Path | Scope | Est. Effort |
|---|---|---|---|
| `clipboard.cpp` | `src/platform/clipboard.{h,cpp}` | Cross-platform clipboard (Linux/Wayland/X11, macOS, Windows). CEO-voice input per Arisa CLAUDE.md rule #3 (no filtering). | 1 day |
| `sanitize.cpp` | `src/safety/sanitize.{h,cpp}` | UTF-8 zero-width removal, full-width space normalisation, HTML tag stripping, URL/mention/hashtag removal, whitespace collapse. Reuse Hatch. safety pattern corpus where applicable (RD-13). | 1 day |
| `db.cpp` | `src/persist/session_store.{h,cpp}` | SQLite3 session store for conversation history. Schema aligned with Traveler. REQ-LLM-MODE-3 (§6.9) and Cockpit lazy-mount snapshot (A-8). Not a direct port; redesign for Traveler. domain. | 2 days |
| `llm.cpp` | `src/llm/provider.{h,cpp}` + plugins | HTTP client for cloud LLM APIs. Must conform to A-10 "thin interface abstraction" with provider plugins for Anthropic / OpenAI / Google + offline llama.cpp. Not a direct port; redesign for plugin architecture. | 3 days |
| **Total** | | | **~1 week** |

**WB-02 Timeline Adjustment:**
- Original WB-02 (inline-copy): 0.5 day
- Adjusted WB-02 (clean-room): 1 week
- Buffer absorption: Phase 0 buffer per Proposal §11.4 R-T04 mitigation is designed for exactly this scenario.

---

## 7. Open Blockers

| # | Blocker | Owner | Resolution Path |
|---|---|---|---|
| B-1 | Clara inline-copy cannot be verified | CTO/PM | STOPPED indefinitely under current-resource rule. Do not copy Clara source into Traveler. |
| B-2 | Clean-room work not yet scoped | Traveler. PM/DM | Scope only when the relevant Traveler. modules are implemented. Use Traveler. requirements, not Clara source. |
| B-3 | Arisa. note may need updating | Arisa. PM | Optional follow-up; not a Traveler. GATE-P0-0 blocker. |

---

## 8. Evidence Checklist

| # | Check | Result | Evidence |
|---|---|---|---|
| 1 | Clara repo inspected | ✅ | `/home/yuma/Clara` — present, no LICENSE |
| 2 | Arisa repo inspected | ✅ | `/home/yuma/arisa` — no Clara code, no LICENSE for Clara |
| 3 | Four target modules located | ✅ | `src/llm.cpp`, `src/db.cpp`, `src/clipboard.cpp`, `src/sanitize.cpp` |
| 4 | Per-module headers read | ✅ | All 8 files (4 cpp + 4 h) read; no SPDX or license block |
| 5 | License grep scan | ✅ | Zero license-related matches in `src/`, `Makefile`, `Makefile.win32` |
| 6 | Prior audit cross-checked | ✅ | Arisa `docs/gate-p1-0/clara-license-status.md` confirms same findings |
| 7 | Spec compliance | ✅ | Spec §4 T0-B Method steps 1–4 followed; conclusion documented |

---

*Document produced per Traveler. Phase 0 Spec v0.1 §4 T0-B Evidence Required.*
*FALLBACK verdict is based solely on verified license evidence. No speculative assumptions about CEO intent were made.*
