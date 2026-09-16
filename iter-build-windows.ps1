<#
.SYNOPSIS
    Fast iteration build for JoltPhysics.js on Windows: rebuilds ONE jolt artifact,
    incremental, for the tight C++ edit/recompile/reload loop against the Nilo dev server.

.DESCRIPTION
    Differences from build-windows.ps1 (the publish-quality script):

      * Single CMake configuration (Debug or Release/Distribution), ST only.
      * Builds a single target (jolt-wasm for Debug, jolt-wasm-compat for Release).
      * Does NOT rm -rf dist/. Ninja's incremental rebuild stays valid across runs.
      * Configures once per Build/Iter/<Variant>/ST dir; subsequent runs just re-`cmake --build`.

    Output format depends on -Variant:

      * Debug (default) -> jolt-wasm target -> dist\jolt-physics.wasm.js + dist\jolt-physics.wasm.wasm
        Non-compat (separate .wasm sidecar) is REQUIRED for usable C++ debugging in Chrome
        DevTools. The wasm-compat single-file format embeds the entire WASM (with DWARF this is
        ~30 MB) as a base64 string literal in the JS, producing a 40+ MB JS source that crashes
        the DevTools renderer when setting C++ breakpoints (the DWARF extension has to keep the
        WASM bytes, JS source, AND DWARF index resident at once). The non-compat form keeps the
        JS glue ~1 MB and exposes the WASM as a first-class binary resource, which DevTools and
        the C/C++ DWARF extension handle natively.

      * Release / Distribution -> jolt-wasm-compat target -> dist\jolt-physics.wasm-compat.js
        Wasm-compat matches Nilo's standard production runtime shape; useful for performance
        comparisons or testing the same artifact format that ships in the npm package.

    Closure (the emcc --closure=1 pass) is ENABLED by default. It costs ~30 s per link.
    For Debug (non-compat), closure mainly minifies the small JS glue (~5-10 MB unminified
    -> ~1 MB minified). DevTools is happy either way because the WASM lives in its own file.
    For Release (wasm-compat), closure is the difference between DevTools surviving and OOM-ing,
    because the JS source contains the embedded WASM blob.

    Typical inner loop, after one-time setup:

        # Edit a Jolt .cpp in C:\dev\JoltPhysics
        .\iter-build-windows.ps1                        # closure on: ~30-60 s/iter
        .\iter-build-windows.ps1 -FastLink              # closure off: ~5-20 s/iter (Debug-safe)
        # Hard refresh the Nilo browser tab

    First run (cold configure + full link) is closer to 1-3 min in both modes.

.PARAMETER Variant
    Debug (default) emits the non-compat dist\jolt-physics.wasm{.js,.wasm} pair with DWARF +
    assertions. This is the C++ debugging-friendly format. Vite's local+debug alias resolves
    to jolt-physics.wasm.js (set NILO_JOLT_LOCAL_DIST + NILO_JOLT_DEBUG=true in .env.local).

    Release / Distribution emits dist\jolt-physics.wasm-compat.js (single-file embedded WASM),
    matching the publish-quality format. Vite's local+release alias resolves to that file.

.PARAMETER JoltPhysicsPath
    Path to the Jolt C++ repo root (folder containing Build/). Defaults to
    $env:JOLT_PHYSICS_PATH or ..\JoltPhysics if either is a valid Jolt clone.

.PARAMETER EmsdkRoot
    Emscripten SDK directory containing emsdk_env.ps1. Defaults to $env:EMSDK, or a sibling
    ..\emsdk clone if present.

.PARAMETER FastLink
    Skip Google Closure (--closure=1). Saves ~30 s/iter at the cost of an unminified JS glue.

    For Debug (non-compat): SAFE. The unminified JS glue is ~5-10 MB; DevTools handles it
    fine because the WASM is in a separate file. Good default for an iteration session.

    For Release (wasm-compat): RISKY. The unminified JS glue plus the embedded 30 MB WASM
    blob crashes DevTools' Sources panel on pause inside Jolt. Only enable when DevTools is
    closed for the session.

    Default OFF (closure runs) so the Release iter path is always DevTools-safe by default.
    For the fastest Debug loop, pass -FastLink.

.PARAMETER Reconfigure
    Force a fresh CMake configure (deletes Build\Iter\<Variant>\ST first). Use after changing
    CMakeLists.txt flags or after a Jolt C++ branch switch that touches CMake files.

.PARAMETER SkipGitCheckout
    Don't try to git checkout NILO_JOLT_TAG. The iteration loop expects a dirty working tree
    almost by definition, so this defaults to ON. Pass -SkipGitCheckout:$false to opt back in.

.NOTES
    Vite consumes the output via NILO_JOLT_LOCAL_DIST=C:/dev/JoltPhysics.js/dist. The alias
    inside vite.config.js is:

      Debug   (NILO_JOLT_DEBUG=true)  -> jolt-physics.wasm.js (+ sidecar jolt-physics.wasm.wasm)
      Release (NILO_JOLT_DEBUG unset) -> jolt-physics.wasm-compat.js

    Each variant uses a distinct filename, so the two builds can coexist in dist/ without a
    rename step and there is no "Debug bytes loaded as Release" footgun: Vite simply errors if
    the expected file for the current NILO_JOLT_DEBUG state is missing.
#>
[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet("Debug", "Release", "Distribution")]
    [string] $Variant = "Debug",

    [Parameter()]
    [string] $JoltPhysicsPath = "",

    [Parameter()]
    [string] $EmsdkRoot = "",

    [Parameter()]
    [switch] $FastLink,

    [Parameter()]
    [switch] $Reconfigure,

    [Parameter()]
    [switch] $SkipGitCheckout = $true
)

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot

function Resolve-JoltCppPath {
    param([string] $Hint)
    if (-not [string]::IsNullOrWhiteSpace($Hint)) { return $Hint }
    if (-not [string]::IsNullOrWhiteSpace($env:JOLT_PHYSICS_PATH)) { return $env:JOLT_PHYSICS_PATH }
    $sibling = Join-Path (Split-Path -Parent $here) "JoltPhysics"
    if (Test-Path -LiteralPath (Join-Path $sibling "Build\CMakeLists.txt")) { return $sibling }
    return ""
}

$JoltPhysicsPath = Resolve-JoltCppPath -Hint $JoltPhysicsPath
if ([string]::IsNullOrWhiteSpace($JoltPhysicsPath) -or
    -not (Test-Path -LiteralPath (Join-Path $JoltPhysicsPath "Build\CMakeLists.txt"))) {
    Write-Error @"
Could not locate a Jolt C++ repo root.

Either:
  - pass -JoltPhysicsPath C:\dev\JoltPhysics
  - set `$env:JOLT_PHYSICS_PATH = 'C:\dev\JoltPhysics'
  - clone JoltPhysics next to this repo at ..\JoltPhysics
"@
}
$coreResolved = (Resolve-Path -LiteralPath $JoltPhysicsPath).Path

function Resolve-EmsdkRoot {
    param([string] $Hint)
    if (-not [string]::IsNullOrWhiteSpace($Hint)) { return $Hint }
    if (-not [string]::IsNullOrWhiteSpace($env:EMSDK)) { return $env:EMSDK }
    # Auto-detect a sibling emsdk clone next to JoltPhysics.js (e.g. C:\dev\emsdk).
    # Mirrors the sibling-clone convention used for the JoltPhysics C++ path.
    $sibling = Join-Path (Split-Path -Parent $here) "emsdk"
    if (Test-Path -LiteralPath (Join-Path $sibling "emsdk_env.ps1")) { return $sibling }
    return ""
}

$EmsdkRoot = Resolve-EmsdkRoot -Hint $EmsdkRoot
if ([string]::IsNullOrWhiteSpace($EmsdkRoot)) {
    Write-Error @"
Could not locate an emsdk install.

Tried (in order):
  - -EmsdkRoot argument
  - `$env:EMSDK
  - sibling clone at $(Join-Path (Split-Path -Parent $here) 'emsdk')

Either:
  - pass -EmsdkRoot C:\dev\emsdk
  - set `$env:EMSDK = 'C:\dev\emsdk'
  - clone emsdk next to this repo at ..\emsdk
"@
}
$emsdkEnv = Join-Path $EmsdkRoot.TrimEnd('\', '/') "emsdk_env.ps1"
if (-not (Test-Path -LiteralPath $emsdkEnv)) {
    Write-Error "EmsdkRoot invalid (missing emsdk_env.ps1): $emsdkEnv"
}
. $emsdkEnv

foreach ($tool in @("emcmake", "cmake", "ninja", "npx")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "$tool not on PATH after emsdk_env (install CMake, Ninja, Node.js)."
    }
}

$buildDir = "Build/Iter/$Variant/ST"
# IMPORTANT: do not assign to a variable named `$fastLink` (or any case variant). PowerShell
# variable names are case-insensitive, so `$fastLink` and the `[switch] $FastLink` param
# refer to the same storage. Assigning a String to a [switch]-typed variable raises
# "Cannot convert value 'System.String' to type 'SwitchParameter'". Use a distinct name.
$fastLinkArg = if ($FastLink) { "ON" } else { "OFF" }
$joltArg = "-DJOLT_PHYSICS_PATH=$($coreResolved -replace '\\', '/')"

# Variant -> target + expected output(s). The two variants use DIFFERENT filenames, so they
# can coexist in dist/ without a rename step:
#
#   Debug   -> jolt-wasm        -> dist\jolt-physics.wasm.js + dist\jolt-physics.wasm.wasm
#   Release -> jolt-wasm-compat -> dist\jolt-physics.wasm-compat.js
#
# BUILD_WASM_COMPAT_ONLY MUST be OFF for jolt-wasm to be a defined target in CMakeLists.txt.
# Setting it OFF unconditionally costs us nothing at build time because `--target` scopes the
# build to a single artifact; it just registers the jolt-wasm command at configure time.
$compatOnly = "OFF"
if ($Variant -eq "Debug") {
    $buildTarget = "jolt-wasm"
    $expectedFiles = @(
        (Join-Path $here "dist\jolt-physics.wasm.js"),
        (Join-Path $here "dist\jolt-physics.wasm.wasm")
    )
    $primaryOutFile = $expectedFiles[0]
} else {
    $buildTarget = "jolt-wasm-compat"
    $expectedFiles = @(
        (Join-Path $here "dist\jolt-physics.wasm-compat.js")
    )
    $primaryOutFile = $expectedFiles[0]
}

Push-Location $here
try {
    if (-not (Test-Path "dist")) {
        New-Item -ItemType Directory -Path "dist" | Out-Null
    }

    $cmakeFlagsFile = Join-Path $buildDir ".nilo-iter-flags"
    $expectedFlags = "variant=$Variant;target=$buildTarget;compatOnly=$compatOnly;fastlink=$fastLinkArg;jolt=$coreResolved"

    $needsConfigure = $Reconfigure -or -not (Test-Path -LiteralPath (Join-Path $buildDir "CMakeCache.txt"))
    if (-not $needsConfigure -and (Test-Path -LiteralPath $cmakeFlagsFile)) {
        $cached = (Get-Content -Raw -LiteralPath $cmakeFlagsFile).Trim()
        if ($cached -ne $expectedFlags) {
            Write-Host "Iter flags changed since last configure:`n  was:  $cached`n  now:  $expectedFlags"
            $needsConfigure = $true
        }
    }
    if ($needsConfigure -and (Test-Path -LiteralPath $buildDir)) {
        Write-Host "Reconfigure: wiping $buildDir"
        Remove-Item -Recurse -Force -LiteralPath $buildDir
    }

    if ($needsConfigure) {
        Write-Host "=== Configure: $buildDir (Variant=$Variant, Target=$buildTarget, FastLink=$fastLinkArg) ==="
        $cmakeArgs = @(
            "-S", ".",
            "-B", $buildDir,
            "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=$Variant",
            "-DBUILD_WASM_COMPAT_ONLY=$compatOnly",
            "-DENABLE_MULTI_THREADING=OFF",
            "-DJPH_DEV_FAST_LINK=$fastLinkArg",
            $joltArg
        )
        & emcmake cmake @cmakeArgs
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed (exit $LASTEXITCODE)" }
        New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
        Set-Content -NoNewline -Path $cmakeFlagsFile -Value $expectedFlags -Encoding utf8
    }

    Write-Host "=== Build: $buildDir --target $buildTarget (incremental) ==="
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    cmake --build $buildDir --target $buildTarget --parallel
    if ($LASTEXITCODE -ne 0) { throw "cmake --build failed (exit $LASTEXITCODE)" }
    $sw.Stop()

    foreach ($f in $expectedFiles) {
        if (-not (Test-Path -LiteralPath $f)) {
            throw "Build did not produce $f. Did CMake change the output target?"
        }
    }

    # Defensive cleanup: when iter-build is invoked for Debug, delete any stale
    # jolt-physics.debug.wasm-compat.js left over from older iter-build versions (which used to
    # emit that file). Otherwise the user could be confused about which artifact is current.
    # No corresponding cleanup for Release; .wasm.* sidecars from a previous Debug iter coexist
    # with .wasm-compat.js cleanly because the Vite alias picks the file matching NILO_JOLT_DEBUG.
    if ($Variant -eq "Debug") {
        $legacyDebug = Join-Path $here "dist\jolt-physics.debug.wasm-compat.js"
        if (Test-Path -LiteralPath $legacyDebug) {
            Write-Host "Cleanup: removing legacy $legacyDebug (replaced by jolt-physics.wasm.* pair)"
            Remove-Item -Force -LiteralPath $legacyDebug
        }
    }

    $totalSizeMb = [Math]::Round((($expectedFiles | ForEach-Object { (Get-Item -LiteralPath $_).Length } | Measure-Object -Sum).Sum) / 1MB, 1)
    $distUrl = "$($here -replace '\\', '/')/dist"
    # `pnpm dev` always; the Variant is picked up from .env.local via NILO_JOLT_DEBUG, not a script alias.
    $pnpmCmd = "pnpm dev"
    $aliasTarget = Split-Path -Leaf $primaryOutFile
    $closureStatus = if ($FastLink) {
        if ($Variant -eq "Debug") {
            "OFF (-FastLink): JS glue unminified, ~5-10 MB. DevTools-safe (WASM is a separate file)."
        } else {
            "OFF (-FastLink): JS glue unminified, ~5-10 MB on top of the embedded 30 MB WASM. WARNING: DevTools Sources panel may OOM if execution pauses inside Jolt. Run without -FastLink for DevTools-safe build."
        }
    } else {
        "ON: JS glue closure-minified, ~1-2 MB. DevTools-safe."
    }

    Write-Host ""
    Write-Host ("=== Done in {0:N1} s. Output: {1} ({2} MB total) ===" -f $sw.Elapsed.TotalSeconds, ($expectedFiles -join ", "), $totalSizeMb)
    Write-Host "Closure: $closureStatus"
    Write-Host ""
    Write-Host "To use this build in Nilo, point Vite at it via NILO_JOLT_LOCAL_DIST."
    if ($Variant -eq "Debug") {
        Write-Host "IMPORTANT: this is the Debug variant (non-compat .wasm.js + .wasm.wasm pair)."
        Write-Host "Set NILO_JOLT_DEBUG=true alongside NILO_JOLT_LOCAL_DIST, otherwise Vite will"
        Write-Host "resolve jolt-physics.wasm-compat.js (Release filename, missing now) and error out."
        Write-Host ""
        Write-Host "Both files (jolt-physics.wasm.js and jolt-physics.wasm.wasm) must be reachable"
        Write-Host "from Vite's dev server. vite.config.js auto-adds NILO_JOLT_LOCAL_DIST to"
        Write-Host "server.fs.allow when set, so the .wasm.wasm sidecar can be fetched at runtime."
    } else {
        Write-Host "IMPORTANT: this is the Release variant (wasm-compat single file)."
        Write-Host "Leave NILO_JOLT_DEBUG unset (or comment it out); setting it to true would"
        Write-Host "resolve the .wasm.* pair (from a previous Debug iter, or missing entirely)."
    }
    Write-Host ""
    Write-Host "Pick ONE of the following (both work; `.env.local` is the daily-driver choice):"
    Write-Host ""
    Write-Host "  [recommended] Persistent - add this line to C:\dev\Nilo\.env.local"
    Write-Host "  (gitignored; survives shell restarts; one-time setup):"
    Write-Host ""
    Write-Host "    NILO_JOLT_LOCAL_DIST=$distUrl"
    Write-Host ""
    Write-Host "  [transient] Per-shell - only for the PowerShell window that starts Vite:"
    Write-Host ""
    Write-Host "    `$env:NILO_JOLT_LOCAL_DIST = '$distUrl'"
    Write-Host ""
    Write-Host "Then (re)start Vite from C:\dev\Nilo:"
    Write-Host "    $pnpmCmd     # Vite alias resolves jolt-physics to local $aliasTarget"
    Write-Host ""
    Write-Host "Vite reads NILO_JOLT_LOCAL_DIST only at server start, so changes to .env.local"
    Write-Host "require a Vite restart. Subsequent re-runs of iter-build-windows.ps1 do NOT require"
    Write-Host "a Vite restart - just hard-refresh the browser tab to pick up the rebuilt wasm."
}
finally {
    Pop-Location
}
