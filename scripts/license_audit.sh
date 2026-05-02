#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ALLOWLIST_FILE="$SCRIPT_DIR/license_allowlist.txt"
METADATA_FILE="$SCRIPT_DIR/license_metadata.txt"
XMAKE_FILE="$SCRIPT_DIR/../xmake.lua"

if [[ ! -f "$ALLOWLIST_FILE" ]]; then
  echo "ERROR: allowlist file not found: $ALLOWLIST_FILE"
  exit 1
fi

if [[ ! -f "$METADATA_FILE" ]]; then
  echo "ERROR: metadata file not found: $METADATA_FILE"
  exit 1
fi

if [[ ! -f "$XMAKE_FILE" ]]; then
  echo "ERROR: xmake.lua not found: $XMAKE_FILE"
  exit 1
fi

is_allowed() {
  local spdx="$1"
  grep -v '^#' "$ALLOWLIST_FILE" | grep -v '^$' | grep -Fxq "$spdx"
}

lookup_metadata() {
  local dep="$1"
  awk -v dep="$dep" '$1 == dep {print $2}' "$METADATA_FILE"
}

failures=0

# 1. Scan xmake.lua dependencies
echo "=== Scanning xmake.lua dependencies ==="
deps=$(grep -oE 'add_requires\s*\([^)]+\)' "$XMAKE_FILE" | sed 's/add_requires(//;s/)$//' | tr ',' '\n' | sed 's/[" ]//g' | grep -v '^$' || true)

if [[ -z "$deps" ]]; then
  echo "No add_requires dependencies found in xmake.lua"
fi

while IFS= read -r dep; do
  spdx=$(lookup_metadata "$dep" || true)
  if [[ -z "$spdx" ]]; then
    echo "FAIL: $dep — no SPDX in metadata"
    failures=$((failures+1))
    continue
  fi
  if is_allowed "$spdx"; then
    echo "PASS: $dep ($spdx)"
  else
    echo "FAIL: $dep ($spdx) — not in allowlist"
    failures=$((failures+1))
  fi
done <<< "$deps"

# 2. Scan third_party submodules
echo "=== Scanning third_party submodules ==="
if [[ -d "$SCRIPT_DIR/../third_party" ]]; then
  for submodule in "$SCRIPT_DIR"/../third_party/*/; do
    if [[ ! -d "$submodule" ]]; then continue; fi
    name=$(basename "$submodule")
    spdx=$(lookup_metadata "$name" || true)
    if [[ -z "$spdx" ]]; then
      license_file=""
      for f in "$submodule/LICENSE" "$submodule/LICENSE.txt" "$submodule/LICENSE.md"; do
        if [[ -f "$f" ]]; then license_file="$f"; break; fi
      done
      if [[ -n "$license_file" ]]; then
        spdx=$(grep -oE 'SPDX-License-Identifier:\s*\S+' "$license_file" | awk '{print $2}' | head -n 1 || true)
      fi
    fi
    if [[ -z "$spdx" ]]; then
      echo "FAIL: $name — cannot determine license"
      failures=$((failures+1))
      continue
    fi
    if is_allowed "$spdx"; then
      echo "PASS: $name ($spdx)"
    else
      echo "FAIL: $name ($spdx) — not in allowlist"
      failures=$((failures+1))
    fi
  done
else
  echo "No third_party directory found"
fi

# 3. Summary
echo "================================"
if [[ $failures -gt 0 ]]; then
  echo "LICENSE AUDIT FAILED: $failures violation(s)"
  exit 1
else
  echo "LICENSE AUDIT PASSED"
  exit 0
fi
