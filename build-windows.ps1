<#
.SYNOPSIS
    Configure + build JoltPhysics.js on Windows (Emscripten), using a local Jolt core tree instead of FetchContent Git.

.DESCRIPTION
    Default -BuildType Distribution matches ./build.sh with no args, and builds the same four passes:

      Build/Debug/SidecarST  -DBUILD_WASM_SIDECAR_ONLY=ON -DJPH_FULL_DWARF=ON  -> ./debug-wasm
      Build/Debug/ST         -DBUILD_WASM_COMPAT_ONLY=ON                       -> ./debug-wasm-compat
      Build/<Type>/ST                                                          -> ./wasm, ./wasm-compat
      Build/<Type>/MT        -DENABLE_MULTI_THREADING=ON                       -> ./wasm-multithread, ...

    ...then d.ts shims and the Examples/js copy. CMAKE_BUILD_TYPE=Debug gives the .debug infix, so the
    Debug and Release artifacts coexist in one dist/ with no renaming. Build dir names match build.sh
    on purpose: every exported flavour must be produced by BOTH scripts or a Windows-built package
    ships a dangling export. Only the release multi-threaded flavour is built; debug MT is not.

    Prefer the non-compat debug build (debug-wasm) for C++ debugging: the
    wasm-compat debug variants embed a multi-MB base64 WASM blob in JS, which crashes Chrome DevTools
    when setting C++ breakpoints (the DWARF extension can't keep all three of: WASM bytes, JS source, and DWARF
    index resident at once). The non-compat debug build keeps JS glue ~1 MB and exposes WASM as a first-class
    binary, which DevTools handles natively. The compat debug variants are still shipped as entry points
    for hosts that can't serve a separate .wasm sidecar.

    The resulting dist/ is suitable for a single npm publish (release + debug entrypoints together).

.PARAMETER JoltPhysicsPath
    Path to the JoltPhysics C++ repo root (folder containing Build/). Passed to CMake as -DJOLT_PHYSICS_PATH=...
    Optional if $env:JOLT_PHYSICS_PATH is set, or if a sibling folder ..\JoltPhysics exists.

.PARAMETER BuildType
    Distribution (default), Release, or Debug - same primary CMAKE_BUILD_TYPE as ./build.sh $1.
    Debug skips the non-compat debug preamble (same as build.sh when BUILD_TYPE=Debug; the primary build
    already emits .debug.wasm.* from CMAKE_BUILD_TYPE=Debug in that case).

.PARAMETER EmsdkRoot
    emsdk directory containing emsdk_env.ps1. Defaults to $env:EMSDK.

.PARAMETER NiloJoltTag
    Git tag checked out in BOTH this repo (JoltPhysics.js) and the JoltPhysics (C++) clone before building.
    Default nilo-v5.5.0; override with -NiloJoltTag or $env:NILO_JOLT_TAG.

.PARAMETER SkipGitCheckout
    Do not run git fetch/checkout (e.g. non-git export).

.NOTES
    Mirrors build.sh for Windows (Ninja + cmd-safe codegen). Nilo uses paired tags (default nilo-v5.5.0).
#>
[CmdletBinding()]
param(
    [Parameter()]
    [string] $JoltPhysicsPath = "",

    [Parameter()]
    [ValidateSet("Debug", "Release", "Distribution")]
    [string] $BuildType = "Distribution",

    [Parameter()]
    [string] $EmsdkRoot = "",

    [Parameter()]
    [string] $NiloJoltTag = "",

    [Parameter()]
    [switch] $SkipGitCheckout
)

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($NiloJoltTag)) {
    if (-not [string]::IsNullOrWhiteSpace($env:NILO_JOLT_TAG)) {
        $NiloJoltTag = $env:NILO_JOLT_TAG
    }
    else {
        $NiloJoltTag = "nilo-v5.5.0"
    }
}

function Resolve-GitExecutable {
    $cmd = Get-Command git -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    $fallback = "C:\Program Files\Git\bin\git.exe"
    if (Test-Path -LiteralPath $fallback) {
        return $fallback
    }
    return $null
}

function Invoke-NiloPairedGitCheckout {
    param(
        [string] $GitExe,
        [string] $RepoPath,
        [string] $Tag
    )
    if ($SkipGitCheckout) {
        Write-Host "SkipGitCheckout: leaving $RepoPath branch/tag unchanged."
        return
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RepoPath ".git"))) {
        Write-Warning "No .git under $RepoPath - skipped git checkout $Tag"
        return
    }
    Write-Host "Git checkout $Tag in $RepoPath"
    & $GitExe -C $RepoPath fetch --tags --prune 2>$null
    & $GitExe -C $RepoPath checkout $Tag
    if ($LASTEXITCODE -ne 0) {
        Write-Error @"
git checkout $Tag failed in $RepoPath.

Create the tag on both forks (same name), push to origin, then:
  git fetch --tags origin

Or pass -SkipGitCheckout if you intentionally build a dirty tree.
"@
    }
    $rev = (& $GitExe -C $RepoPath rev-parse --short HEAD).Trim()
    Write-Host "  -> HEAD $rev"
}

if ([string]::IsNullOrWhiteSpace($JoltPhysicsPath)) {
    if (-not [string]::IsNullOrWhiteSpace($env:JOLT_PHYSICS_PATH)) {
        $JoltPhysicsPath = $env:JOLT_PHYSICS_PATH
    }
    else {
        $sibling = Join-Path (Split-Path -Parent $here) "JoltPhysics"
        if (Test-Path -LiteralPath (Join-Path $sibling "Build\CMakeLists.txt")) {
            $JoltPhysicsPath = $sibling
        }
    }
}
if ([string]::IsNullOrWhiteSpace($JoltPhysicsPath)) {
    Write-Error @"
JoltPhysicsPath is required. Use -JoltPhysicsPath, or set `$env:JOLT_PHYSICS_PATH, or clone Jolt next to this repo.

Examples:
  .\build-windows.ps1 -JoltPhysicsPath C:\dev\JoltPhysics
  `$env:JOLT_PHYSICS_PATH = 'C:\dev\JoltPhysics'; .\build-windows.ps1
"@
}
if (-not (Test-Path -LiteralPath $JoltPhysicsPath)) {
    Write-Error "JoltPhysicsPath does not exist: $JoltPhysicsPath"
}
$coreResolved = (Resolve-Path -LiteralPath $JoltPhysicsPath).Path
if (-not (Test-Path -LiteralPath (Join-Path $coreResolved "Build\CMakeLists.txt"))) {
    Write-Error "JoltPhysicsPath must point at Jolt repo root (missing Build\CMakeLists.txt): $coreResolved"
}

$gitExe = Resolve-GitExecutable
if (-not $gitExe) {
    Write-Warning "git not found; paired tag checkout skipped. Install Git for Windows or add git to PATH."
}
else {
    Invoke-NiloPairedGitCheckout -GitExe $gitExe -RepoPath $coreResolved -Tag $NiloJoltTag
    Invoke-NiloPairedGitCheckout -GitExe $gitExe -RepoPath $here -Tag $NiloJoltTag
}

$characterVirtualHeader = Join-Path $coreResolved "Jolt\Physics\Character\CharacterVirtual.h"
if (Test-Path -LiteralPath $characterVirtualHeader) {
    $cv = Get-Content -Raw -LiteralPath $characterVirtualHeader
    if ($cv -match "OnContactAdded\(const CharacterVirtual \*inCharacter, const CharacterContact &inContact" -and
        $cv -notmatch "CharacterVirtual::Contact") {
        Write-Error @"
Jolt core at $coreResolved uses the newer CharacterContact listener API. JoltPhysics.js still targets the v5.5.0-era API.

Fix: git fetch --tags && git checkout nilo-v5.5.0 (or v5.5.0) in your Jolt clone, then re-run.
"@
    }
}

if ([string]::IsNullOrWhiteSpace($EmsdkRoot)) {
    $EmsdkRoot = $env:EMSDK
}
if ([string]::IsNullOrWhiteSpace($EmsdkRoot)) {
    Write-Error @"
EmsdkRoot / EMSDK is empty.

PowerShell:
  `$env:EMSDK = 'C:\dev\emsdk'
  .\build-windows.ps1
"@
}
$emsdkEnv = Join-Path $EmsdkRoot.TrimEnd('\', '/') "emsdk_env.ps1"
if (-not (Test-Path -LiteralPath $emsdkEnv)) {
    Write-Error "EmsdkRoot invalid (missing emsdk_env.ps1): $emsdkEnv. Set EMSDK or pass -EmsdkRoot."
}
. $emsdkEnv

foreach ($tool in @("emcmake", "cmake", "ninja", "npx")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "$tool not on PATH after emsdk_env (install CMake, Ninja, Node.js)."
    }
}

$joltArg = "-DJOLT_PHYSICS_PATH=$($coreResolved -replace '\\', '/')"

function Invoke-EmcmakeBuild {
    param(
        [string] $BuildDir,
        [string] $CMakeBuildType,
        [string[]] $ExtraCmakeArgs,
        [string] $Target = ""
    )
    Write-Host "Configure $BuildDir (CMAKE_BUILD_TYPE=$CMakeBuildType) ..."
    $cmakeArgs = @(
        "-S", ".",
        "-B", $BuildDir,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=$CMakeBuildType"
    ) + $ExtraCmakeArgs + $script:joltCommonArgs
    & emcmake cmake @cmakeArgs
    # ``--verbose`` -> Ninja ``-v``: full ``em++`` lines in logs (large). Remove when done investigating.
    $buildArgs = @("--build", $BuildDir, "--parallel", "--verbose")
    if (-not [string]::IsNullOrEmpty($Target)) {
        $buildArgs += @("--target", $Target)
    }
    cmake @buildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --build failed for $BuildDir (exit $LASTEXITCODE)"
    }
}

$script:joltCommonArgs = @($joltArg)

Push-Location $here
try {
    if (Test-Path "dist") {
        Remove-Item -Recurse -Force "dist"
    }
    New-Item -ItemType Directory -Path "dist" | Out-Null

    # --- Debug preamble: BOTH debug flavours the package exports, mirroring build.sh's two Debug
    # --- passes. Build dir names match build.sh deliberately (Debug/ST = compat, Debug/SidecarST =
    # --- sidecar) so the two scripts can't drift into building different things. CMakeLists derives
    # --- the .debug infix from CMAKE_BUILD_TYPE, so no Move-Item / sed rewrites are needed.
    # --- MT debug is intentionally dropped (no consumers, never shipped).
    if ($BuildType -ne "Debug") {
        # Wipe both Debug dirs: an older layout used Debug/ST for the NON-compat build, so a stale
        # cache there would fight the explicit -D flags below.
        foreach ($d in @("Build\Debug\ST", "Build\Debug\SidecarST")) {
            if (Test-Path $d) { Remove-Item -Recurse -Force $d }
        }

        # ./debug-wasm — the sidecar Nilo sets C++ breakpoints in. Full DWARF; matches build.sh's
        # Build/Debug/SidecarST pass.
        Write-Host "=== Preamble: Debug sidecar ST (jolt-physics.debug.wasm.{js,wasm}) ==="
        Invoke-EmcmakeBuild -BuildDir "Build/Debug/SidecarST" -CMakeBuildType "Debug" -ExtraCmakeArgs @(
            "-DBUILD_WASM_SIDECAR_ONLY=ON",
            "-DENABLE_SIMD=ON",
            "-DJPH_FULL_DWARF=ON"
        )
        foreach ($f in @("dist\jolt-physics.debug.wasm.js", "dist\jolt-physics.debug.wasm.wasm")) {
            if (-not (Test-Path $f)) { throw "Debug sidecar preamble did not produce $f" }
        }

        # ./debug-wasm-compat — source maps, not DWARF (the base64-embedded form crashes DevTools on
        # C++ breakpoints, so it is the fallback for hosts that can't serve a sidecar). Without this
        # pass the Windows package shipped a DANGLING ./debug-wasm-compat export: the d.ts shim and
        # package.json entry existed but the .js was never built. Matches build.sh's Debug/ST pass.
        Write-Host "=== Preamble: Debug compat ST (jolt-physics.debug.wasm-compat.js) ==="
        Invoke-EmcmakeBuild -BuildDir "Build/Debug/ST" -CMakeBuildType "Debug" -ExtraCmakeArgs @(
            "-DBUILD_WASM_COMPAT_ONLY=ON",
            "-DENABLE_SIMD=ON"
        )
        if (-not (Test-Path "dist\jolt-physics.debug.wasm-compat.js")) {
            throw "Debug compat preamble did not produce dist\jolt-physics.debug.wasm-compat.js"
        }
    }

    # --- build.sh: full primary CMAKE_BUILD_TYPE ST then MT ---
    # When -BuildType Debug, this is a developer-convenience mode: CMAKE_BUILD_TYPE=Debug gives every
    # output the .debug infix, so no release-named artifact is produced and dist/ is NOT publish-ready. Use
    # -BuildType Distribution (default) for a publish-quality dist with both Release + Debug artifacts.
    Write-Host "=== Primary: $BuildType ST + MT (full npm dist) ==="
    Invoke-EmcmakeBuild -BuildDir "Build/$BuildType/ST" -CMakeBuildType $BuildType -ExtraCmakeArgs @(
        "-DENABLE_SIMD=ON"
    )
    # Release MT flavour: not used by Nilo, but the threaded Examples import it. Guarded because
    # CMAKE_BUILD_TYPE=Debug here would produce a DEBUG MT build, which neither script ships and
    # which the stale-sweep below deletes -- pure wasted link time. build.sh likewise builds MT
    # only in its non-Debug branch.
    if ($BuildType -ne "Debug") {
        Invoke-EmcmakeBuild -BuildDir "Build/$BuildType/MT" -CMakeBuildType $BuildType -ExtraCmakeArgs @(
            "-DENABLE_MULTI_THREADING=ON",
            "-DENABLE_SIMD=ON"
        )
    }

    # --- d.ts shims (one canonical file, copied to each entrypoint's expected name) ---
    $dts = "import Jolt from ""./types"";`n`nexport default Jolt;`nexport * from ""./types"";`n`n"
    foreach ($name in @(
            "jolt-physics.wasm.d.ts",
            "jolt-physics.wasm-compat.d.ts",
            "jolt-physics.debug.wasm.d.ts",
            "jolt-physics.debug.wasm-compat.d.ts",
            "jolt-physics.multithread.wasm.d.ts",
            "jolt-physics.multithread.wasm-compat.d.ts")) {
        Set-Content -Path "dist\$name" -Value $dts -Encoding utf8
    }

    $ex = Join-Path $here "Examples\js"
    if (Test-Path $ex) {
        Copy-Item -Force "dist\jolt-physics*.wasm-compat.js" $ex
    }

    # Defensive cleanup: drop the asm.js flavour we no longer build at all (the jolt-javascript
    # target is gone now that ST builds with SIMD), plus debug artifacts older publish builds
    # (<= nilo.2) produced under names we no longer use. A developer with an older checkout can
    # have these left behind. If they're left in dist/ they don't ship (not in package.json
    # files), but they confuse `npm pack --dry-run` output.
    #
    # NOTE: the ST *.wasm-compat debug artifact is NOT listed here — debug-wasm-compat
    # is an exported entry point, so deleting it would leave a
    # Windows-built package with a dangling export. The DEBUG multithread artifacts ARE listed:
    # the fork builds and ships only the release MT flavour, so debug-MT leftovers should be swept.
    foreach ($stale in @(
            "dist\jolt-physics.debug.js",
            "dist\jolt-physics.debug.d.ts",
            "dist\jolt-physics.js",
            "dist\jolt-physics.d.ts",
            "dist\jolt-physics.multithread.d.ts",
            "dist\jolt-physics.debug.multithread.wasm.js",
            "dist\jolt-physics.debug.multithread.wasm.d.ts",
            "dist\jolt-physics.debug.multithread.wasm.wasm",
            "dist\jolt-physics.debug.multithread.wasm.wasm.map",
            "dist\jolt-physics.debug.multithread.wasm-compat.js",
            "dist\jolt-physics.debug.multithread.wasm-compat.d.ts")) {
        if (Test-Path -LiteralPath $stale) {
            Write-Host "Cleanup: removing stale $stale (no longer shipped)"
            Remove-Item -Force -LiteralPath $stale
        }
    }

    # Validate dist matches package.json "files" (dist entries + types.d.ts).
    # Debug-named artifacts only exist when a Debug-non-compat preamble ran (i.e. BuildType != Debug).
    # -BuildType Debug is a developer-convenience mode: CMAKE_BUILD_TYPE=Debug gives every output the
    # .debug infix, so NO release-named artifact is produced and dist/ is not publish-ready. Only the
    # publish path (non-Debug) can assert the full package contract.
    $requiredDist = @("dist\types.d.ts")
    if ($BuildType -ne "Debug") {
        $requiredDist += @(
            "dist\jolt-physics.wasm-compat.js",
            "dist\jolt-physics.wasm-compat.d.ts",
            "dist\jolt-physics.wasm.js",
            "dist\jolt-physics.wasm.d.ts",
            "dist\jolt-physics.wasm.wasm",
            "dist\jolt-physics.multithread.wasm-compat.js",
            "dist\jolt-physics.multithread.wasm-compat.d.ts",
            "dist\jolt-physics.multithread.wasm.js",
            "dist\jolt-physics.multithread.wasm.d.ts",
            "dist\jolt-physics.multithread.wasm.wasm",
            "dist\jolt-physics.debug.wasm.js",
            "dist\jolt-physics.debug.wasm.d.ts",
            "dist\jolt-physics.debug.wasm.wasm",
            "dist\jolt-physics.debug.wasm-compat.js",
            "dist\jolt-physics.debug.wasm-compat.d.ts"
        )
    }
    $missing = @()
    foreach ($p in $requiredDist) {
        if (-not (Test-Path -LiteralPath $p)) {
            $missing += $p
        }
    }
    if ($missing.Count -gt 0) {
        throw "dist/ is incomplete for npm publish. Missing:`n  " + ($missing -join "`n  ")
    }

    Write-Host "Done. dist/ is ready for npm pack / npm publish. ($($requiredDist.Count) expected artifacts present)"
}
finally {
    Pop-Location
}
