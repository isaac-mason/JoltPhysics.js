# Nilo fork additions to JoltPhysics.js's CMakeLists.txt.
#
# All Nilo-specific build logic lives here so future merges from upstream JoltPhysics.js
# touch only a small set of breadcrumbs in CMakeLists.txt itself. Read this file
# end-to-end to understand what the fork customizes; the root CMakeLists.txt is kept
# close to upstream and only contains:
#   - one `include(cmake/NiloFork.cmake)` near the top,
#   - calls to the `nilo_*` macros declared below at the appropriate points,
#   - a handful of in-place value swaps for cross-platform Windows compat (each marked `# NILO:`).

cmake_minimum_required(VERSION 3.13)

# Python helpers for cross-platform glue codegen (replaces upstream's `cat` / `sed`
# invocations so Windows Ninja, which uses cmd.exe, can build without GNU coreutils).
# Used three places in CMakeLists.txt:
#   - REMOVE_THREAD_LOCAL custom command (strip thread_local from glue.cpp on ST builds)
#   - the IDL concat custom command
#   - the post-emcc replace_by_import workaround (emscripten#245)
set(JOLT_BUILD_TOOLS "${CMAKE_CURRENT_SOURCE_DIR}/build-tools/jolt_codegen_helpers.py"
    CACHE INTERNAL "Nilo: path to Python codegen helpers")
set(JOLT_REPLACE_IMPORT
    "${Python3_EXECUTABLE}" "${JOLT_BUILD_TOOLS}" replace-import-token
    CACHE INTERNAL "Nilo: post-emcc replace_by_import workaround command")

# ---------------------------------------------------------------------------
# Output filename suffix
# ---------------------------------------------------------------------------
# `-DJPH_OUTPUT_NAME_SUFFIX=.debug` from build.sh / build-windows.ps1 (Debug preamble)
# lets a single dist/ folder hold both `jolt-physics.wasm.js` and
# `jolt-physics.debug.wasm.js` side-by-side after a publish build. iter-build leaves
# this empty so debug-iter outputs match Vite's local-iter alias filename.
macro(nilo_apply_output_name_suffix)
    if (NOT DEFINED JPH_OUTPUT_NAME_SUFFIX)
        set(JPH_OUTPUT_NAME_SUFFIX "")
    endif()
    set(OUTPUT_BASE_NAME "${OUTPUT_BASE_NAME}${JPH_OUTPUT_NAME_SUFFIX}")
endmacro()

# ---------------------------------------------------------------------------
# JoltPhysics C++ source (FetchContent override)
# ---------------------------------------------------------------------------
# Default: pull Nilo's JoltPhysics C++ fork via Git (paired tag `nilo-vX.Y.Z`).
# Override:
#   -DJOLT_PHYSICS_PATH=C:/dev/JoltPhysics                                # local checkout, skip fetch
#   -DJOLT_PHYSICS_GIT_REPO=https://github.com/<org>/JoltPhysics  -DJOLT_PHYSICS_GIT_TAG=...   # different remote/tag
set(JOLT_PHYSICS_PATH "" CACHE PATH
    "Nilo: local JoltPhysics C++ repo root; when set, skips Git fetch.")
set(JOLT_PHYSICS_GIT_REPO "https://github.com/nilo-technologies/JoltPhysics" CACHE STRING
    "Nilo: FetchContent JoltPhysics C++ Git URL (set to your fork).")
set(JOLT_PHYSICS_GIT_TAG "nilo-v5.5.0" CACHE STRING
    "Nilo: FetchContent tag on JOLT_PHYSICS_GIT_REPO (paired with this repo's nilo-* tag).")

macro(nilo_declare_jolt_physics_fetchcontent)
    include(FetchContent)
    if(JOLT_PHYSICS_PATH)
        get_filename_component(_jph_path_resolved "${JOLT_PHYSICS_PATH}" ABSOLUTE)
        if(NOT EXISTS "${_jph_path_resolved}/Build/CMakeLists.txt")
            message(FATAL_ERROR "JOLT_PHYSICS_PATH does not look like a JoltPhysics repo root (missing Build/CMakeLists.txt): ${_jph_path_resolved}")
        endif()
        FetchContent_Declare(JoltPhysics
            SOURCE_DIR "${_jph_path_resolved}"
            SOURCE_SUBDIR "Build")
    else()
        FetchContent_Declare(JoltPhysics
            GIT_REPOSITORY ${JOLT_PHYSICS_GIT_REPO}
            GIT_TAG ${JOLT_PHYSICS_GIT_TAG}
            SOURCE_SUBDIR "Build")
    endif()
    FetchContent_MakeAvailable(JoltPhysics)
endmacro()

# ---------------------------------------------------------------------------
# Resolve the C++ repo root (`JPH_JOLT_PHYSICS_SOURCE_ROOT`)
# ---------------------------------------------------------------------------
# Sets a canonical absolute path to the JoltPhysics C++ repo root. Used by both
# the DWARF prefix-map flags and the EMCC_GLUE_ARGS `-I` flag. Call after
# `nilo_declare_jolt_physics_fetchcontent()`.
#
# FetchContent populates `joltphysics_SOURCE_DIR` (lowercase, NOT JoltPhysics_SOURCE_DIR).
# When `-DJOLT_PHYSICS_PATH` is set we always prefer that — `joltphysics_SOURCE_DIR` has
# been observed (Linux) to equal the JS workspace root rather than the C++ checkout root,
# which breaks DWARF prefix-map alignment with actual compile paths.
#
# We also force the `Jolt` target's include directories to use the same absolute root, so
# Ninja-emitted `-I` paths match what the prefix maps see (REALPATH collapses any
# `..` components from upstream Jolt's `Build/..`).
macro(nilo_resolve_jolt_physics_source_root)
    if(JOLT_PHYSICS_PATH)
        get_filename_component(JPH_JOLT_PHYSICS_SOURCE_ROOT "${JOLT_PHYSICS_PATH}" ABSOLUTE)
        if(NOT EXISTS "${JPH_JOLT_PHYSICS_SOURCE_ROOT}/Jolt/Jolt.h")
            message(FATAL_ERROR "JOLT_PHYSICS_PATH must be the JoltPhysics C++ repo root (expected Jolt/Jolt.h): ${JPH_JOLT_PHYSICS_SOURCE_ROOT}")
        endif()
    elseif(DEFINED joltphysics_SOURCE_DIR AND NOT "${joltphysics_SOURCE_DIR}" STREQUAL "")
        set(JPH_JOLT_PHYSICS_SOURCE_ROOT "${joltphysics_SOURCE_DIR}")
    elseif(DEFINED JoltPhysics_SOURCE_DIR AND NOT "${JoltPhysics_SOURCE_DIR}" STREQUAL "")
        set(JPH_JOLT_PHYSICS_SOURCE_ROOT "${JoltPhysics_SOURCE_DIR}")
    else()
        message(FATAL_ERROR "FetchContent: expected joltphysics_SOURCE_DIR after MakeAvailable(JoltPhysics)")
    endif()

    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.19")
        get_filename_component(JPH_JOLT_PHYSICS_SOURCE_ROOT "${JPH_JOLT_PHYSICS_SOURCE_ROOT}" REALPATH)
    else()
        get_filename_component(JPH_JOLT_PHYSICS_SOURCE_ROOT "${JPH_JOLT_PHYSICS_SOURCE_ROOT}" ABSOLUTE)
    endif()
    if(TARGET Jolt)
        target_include_directories(Jolt BEFORE PUBLIC "$<BUILD_INTERFACE:${JPH_JOLT_PHYSICS_SOURCE_ROOT}>")
    endif()
endmacro()

# ---------------------------------------------------------------------------
# DWARF prefix maps (Debug builds only)
# ---------------------------------------------------------------------------
# Sets `JPH_DEBUG_PREFIX_MAP_FLAGS` (or empty list in non-Debug) so DWARF source paths
# are portable across build hosts and the Chrome "C/C++ DevTools Support (DWARF)"
# extension can locate sources via simple Path Substitutions.
#
# Two flags work together:
#   1. `-fdebug-prefix-map=<absolute>=<short>` rewrites filenames in DWARF entries:
#         <JoltPhysics.js root>  ->  JoltPhysics.js/
#         <JoltPhysics C++ root> ->  JoltPhysics/
#   2. `-fdebug-compilation-dir=.` overrides DW_AT_comp_dir so it records as "."
#      instead of an absolute CMake binary directory. DevTools concatenates
#      DW_AT_comp_dir + filename before path-substitution lookup, so without this
#      the absolute build dir leaks even when filenames are cleanly rewritten.
#
# We also pair `-fdebug-prefix-map` with `-fmacro-prefix-map` so `__FILE__` expansions
# in JPH_ASSERT/etc. capture relative paths instead of build-host absolute paths
# (otherwise hundreds of runtime path strings leak into the wasm).
#
# IMPORTANT: `-fdebug-prefix-map` rules are NOT applied in command-line order. Clang
# stores entries sorted by source prefix, applies exactly one rule per path (no
# chaining). When two prefixes nest, the shorter (lexicographically first) wins —
# which means a JS-workspace prefix that is a strict prefix of the C++ checkout path
# will eat all Jolt source paths and produce broken `JoltPhysics.js/JoltPhysics/...`
# DWARF regardless of command-line ordering. We therefore HARD-REQUIRE that the two
# repo paths do not nest, and FATAL_ERROR at configure time if they do (CI must clone
# JoltPhysics outside `$GITHUB_WORKSPACE`; locally use sibling clones such as
# `C:/dev/JoltPhysics.js` + `C:/dev/JoltPhysics`).
#
# The resulting list is referenced from CMakeLists.txt at three places: the Jolt
# target compile (handled here), `EMCC_ARGS` (link), and `EMCC_GLUE_ARGS` (glue.cpp
# compile).
macro(nilo_compute_debug_prefix_map_flags)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        get_filename_component(_jph_js_repo  "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
        get_filename_component(_jph_cpp_repo "${JPH_JOLT_PHYSICS_SOURCE_ROOT}" ABSOLUTE)

        string(FIND "${_jph_cpp_repo}/" "${_jph_js_repo}/" _jph_cpp_in_js)
        string(FIND "${_jph_js_repo}/"  "${_jph_cpp_repo}/" _jph_js_in_cpp)
        if (NOT _jph_cpp_in_js EQUAL -1 OR NOT _jph_js_in_cpp EQUAL -1)
            message(FATAL_ERROR
                "JoltPhysics.js Debug DWARF prefix maps require non-nested repo paths.\n"
                "  JoltPhysics.js workspace : ${_jph_js_repo}\n"
                "  JoltPhysics  C++ checkout: ${_jph_cpp_repo}\n"
                "One path is a prefix of the other, so Clang's -fdebug-prefix-map will "
                "map every Jolt source through the wrong rule and produce broken nested "
                "paths in DWARF (e.g. JoltPhysics.js/JoltPhysics/Jolt/...). Move the C++ "
                "checkout to a sibling location (in CI: clone into \$RUNNER_TEMP; "
                "locally: C:/dev/JoltPhysics + C:/dev/JoltPhysics.js as siblings) or "
                "pass -DJOLT_PHYSICS_PATH to a non-nested directory.")
        endif()

        set(JPH_DEBUG_PREFIX_MAP_FLAGS
            "-fdebug-prefix-map=${_jph_cpp_repo}=JoltPhysics"
            "-fmacro-prefix-map=${_jph_cpp_repo}=JoltPhysics"
            "-fdebug-prefix-map=${_jph_js_repo}=JoltPhysics.js"
            "-fmacro-prefix-map=${_jph_js_repo}=JoltPhysics.js"
            "-fdebug-compilation-dir=.")
        if (TARGET Jolt)
            target_compile_options(Jolt PRIVATE ${JPH_DEBUG_PREFIX_MAP_FLAGS})
        endif()
        message(STATUS "JoltPhysics.js Debug DWARF prefix maps:")
        foreach(_jph_map IN LISTS JPH_DEBUG_PREFIX_MAP_FLAGS)
            message(STATUS "  ${_jph_map}")
        endforeach()
    else()
        set(JPH_DEBUG_PREFIX_MAP_FLAGS "")
    endif()
endmacro()

# ---------------------------------------------------------------------------
# Closure pass gating for fast dev iteration (`JPH_DEV_FAST_LINK`)
# ---------------------------------------------------------------------------
# Closure adds ~30 s to the emcc link step (90 %+ of incremental rebuild time once
# Ninja is doing its job on the C++ side). C++ correctness and DWARF debugging are
# unaffected by skipping it.
#
# Default OFF for closure-skip (i.e. closure is ON) because:
#   1. CI / publish builds must match upstream output exactly.
#   2. Closure minifies the JS glue (long identifiers -> short), shrinking the JS
#      source text Chrome DevTools' Sources panel has to render when execution
#      pauses inside Jolt code. Unminified glue is 5-10 MB; with closure ~1-2 MB.
#      Above ~3 MB DevTools' renderer becomes unreliable / OOMs.
#
# Set `JPH_DEV_FAST_LINK=ON` only for iteration sessions where DevTools is closed.
# Sets `JPH_CLOSURE_ARGS` for use in `EMCC_ARGS`.
option(JPH_DEV_FAST_LINK
    "Skip --closure=1 in emcc link to accelerate dev iteration. WARNING: produces a larger JS glue that can crash Chrome DevTools' Sources panel when paused inside Jolt code. Only enable for iteration sessions where DevTools is closed."
    OFF)

macro(nilo_compute_closure_args)
    if (JPH_DEV_FAST_LINK)
        set(JPH_CLOSURE_ARGS "")
        message(STATUS "JPH_DEV_FAST_LINK=ON: skipping --closure=1 in EMCC link (dev iteration mode)")
    else()
        set(JPH_CLOSURE_ARGS
            --closure=1
            --closure-args="--dynamic_import_alias=replace_by_import"
            --closure-args="--externs"
            --closure-args="${CMAKE_CURRENT_SOURCE_DIR}/extern-import.js")
    endif()
endmacro()
