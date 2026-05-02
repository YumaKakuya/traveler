param(
    [string]$AllowlistFile = "$PSScriptRoot/license_allowlist.txt",
    [string]$MetadataFile = "$PSScriptRoot/license_metadata.txt",
    [string]$XmakeFile = "$PSScriptRoot/../xmake.lua"
)

if (-not (Test-Path $AllowlistFile)) {
    Write-Error "Allowlist file not found: $AllowlistFile"
    exit 1
}
if (-not (Test-Path $MetadataFile)) {
    Write-Error "Metadata file not found: $MetadataFile"
    exit 1
}
if (-not (Test-Path $XmakeFile)) {
    Write-Error "xmake.lua not found: $XmakeFile"
    exit 1
}

$allowlist = Get-Content $AllowlistFile | Where-Object { $_ -notmatch '^#' -and $_ -match '\S' }

function Is-Allowed($spdx) {
    return $allowlist -contains $spdx
}

function Lookup-Metadata($dep) {
    $line = Get-Content $MetadataFile | Where-Object { ($_ -split '\s+')[0] -eq $dep }
    if ($line) {
        return ($line -split '\s+')[1]
    }
    return $null
}

$failures = 0

# 1. Scan xmake.lua dependencies
Write-Host "=== Scanning xmake.lua dependencies ==="
$content = Get-Content $XmakeFile -Raw
$matchesList = [regex]::Matches($content, 'add_requires\s*\(([^)]+)\)')
$deps = @()
foreach ($m in $matchesList) {
    $inner = $m.Groups[1].Value
    $parts = $inner -split ','
    foreach ($p in $parts) {
        $clean = $p.Trim().Trim('"').Trim()
        if ($clean -ne '') { $deps += $clean }
    }
}

if ($deps.Count -eq 0) {
    Write-Host "No add_requires dependencies found in xmake.lua"
}

foreach ($dep in $deps) {
    $spdx = Lookup-Metadata $dep
    if (-not $spdx) {
        Write-Host "FAIL: $dep - no SPDX in metadata"
        $failures++
        continue
    }
    if (Is-Allowed $spdx) {
        Write-Host "PASS: $dep ($spdx)"
    } else {
        Write-Host "FAIL: $dep ($spdx) - not in allowlist"
        $failures++
    }
}

# 2. Scan third_party submodules
Write-Host "=== Scanning third_party submodules ==="
$repoRoot = Split-Path $PSScriptRoot -Parent
$thirdParty = Join-Path $repoRoot "third_party"
if (Test-Path $thirdParty) {
    $submodules = Get-ChildItem -Path $thirdParty -Directory
    foreach ($sub in $submodules) {
        $name = $sub.Name
        $spdx = Lookup-Metadata $name
        if (-not $spdx) {
            $licenseFile = $null
            foreach ($f in @("LICENSE","LICENSE.txt","LICENSE.md")) {
                $candidate = Join-Path $sub.FullName $f
                if (Test-Path $candidate) {
                    $licenseFile = $candidate
                    break
                }
            }
            if ($licenseFile) {
                $matchesFound = Select-String -Path $licenseFile -Pattern 'SPDX-License-Identifier:\s*(\S+)'
                if ($matchesFound) {
                    $first = $matchesFound | Select-Object -First 1
                    $spdx = $first.Matches[0].Groups[1].Value
                }
            }
        }
        if (-not $spdx) {
            Write-Host "FAIL: $name - cannot determine license"
            $failures++
            continue
        }
        if (Is-Allowed $spdx) {
            Write-Host "PASS: $name ($spdx)"
        } else {
            Write-Host "FAIL: $name ($spdx) - not in allowlist"
            $failures++
        }
    }
} else {
    Write-Host "No third_party directory found"
}

Write-Host "================================"
if ($failures -gt 0) {
    Write-Host "LICENSE AUDIT FAILED: $failures violation(s)"
    exit 1
} else {
    Write-Host "LICENSE AUDIT PASSED"
    exit 0
}
