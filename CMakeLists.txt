cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(EMSCRIPTEN_ROOT $ENV{EMSDK}/upstream/emscripten CACHE STRING "Emscripten path")
set(CMAKE_TOOLCHAIN_FILE ${EMSCRIPTEN_ROOT}/cmake/Modules/Platform/Emscripten.cmake)
set(JOLT_PRE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pre.js)
set(JOLT_POST_FILE ${CMAKE_CURRENT_SOURCE_DIR}/post.js)
# post.js is a template (carries __JOLT_OUT_META__ / __JOLT_LAYOUT__ tokens). The typing step
# below bakes the binding-site metadata into it and emits this generated file, which is what the
# flavor links actually --post-js. Per build dir; the metadata is identical across that dir's flavors.
set(JOLT_POST_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/post.generated.js)
set(JOLT_NO_FINALIZER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/no-finalizer.js)
set(JOLT_HEADER_FILE ${CMAKE_CURRENT_SOURCE_DIR}/JoltJS.h)
set(JOLT_BINDINGS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/bindings.cpp)
set(OUTPUT_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/dist/)
set(JOLT_TYPING ${OUTPUT_FOLDER}types.d.ts)

# Option to turn on memory profiling
option(ENABLE_MEMORY_PROFILER "Enable emscriptens memory profiler to help find leaks" OFF)

# Option to turn on double precision
option(DOUBLE_PRECISION "Compile the library in double precision mode" OFF)

# Option to turn multi threading on
option(ENABLE_MULTI_THREADING "Compile the library in multi threading mode (warning: doesn't work in Safari nor with any JS callbacks currently)'" OFF)

# Option to turn on SIMD
option(ENABLE_SIMD "Enable SIMD instructions (warning: last major browser to support this was Safari 16.4 in March 2023)" OFF)

# Option to speed up compilation by only compiling the WASM compat version
option(BUILD_WASM_COMPAT_ONLY "Compile only the WASM compat version of the library (speeds up compilation)" OFF)

# Option to build ONLY the separate-.wasm sidecar flavor (jolt-physics[.debug].wasm.js +
# .wasm.wasm) — skips the base64 SINGLE_FILE compat build and the closure .js. Used for a
# debuggable debug build: base64-embedded debug wasm OOMs bundlers (Vercel) and crashes
# Chrome's C++/DWARF breakpoints, so debug also needs a separate-.wasm sidecar.
option(BUILD_WASM_SIDECAR_ONLY "Compile only the separate-.wasm sidecar flavor" OFF)

# Enable for resizable heap, with some amount of slowness
option(ALLOW_MEMORY_GROWTH "Allow Memory Growth" OFF)

# Closure-compile the JS glue (validated to work with embind; toggle off to isolate issues)
option(USE_CLOSURE "Run the closure compiler on the generated JS" ON)

# Default initial memory size is 128*1024*1024 = 128MB
set(INITIAL_MEMORY 134217728 CACHE STRING "Initial Memory")

# Default maximum memory size is 2*1024*1024*1024 = 2GB (only used when ALLOW_MEMORY_GROWTH is true)
set(MAXIMUM_MEMORY 2147483648 CACHE STRING "Maximum Memory")

# The configurations we support
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;Distribution")

# ---- Jolt library configuration ----
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)  # No floating point exception checking
set(INTERPROCEDURAL_OPTIMIZATION OFF)       # Turn off LTO
set(DISABLE_CUSTOM_ALLOCATOR ON)            # Turn off custom allocator
set(ENABLE_OBJECT_STREAM OFF)               # Turn off ObjectStream
set(OBJECT_LAYER_BITS 32)                   # 32-bit object layers for ObjectLayerPairFilterMask
set(CPP_RTTI_ENABLED ON)                    # embind auto-downcasting relies on C++ RTTI

# Override debug symbols
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(GENERATE_DEBUG_SYMBOLS ON)
	set(JPH_DEBUG_SYMBOL_FORMAT "source-map" CACHE STRING "")
else()
	set(GENERATE_DEBUG_SYMBOLS OFF)
endif()

# Enable multi threading
if (ENABLE_MULTI_THREADING)
	set(MULTI_THREADED_FLAG
		-pthread
		--post-js ${CMAKE_CURRENT_SOURCE_DIR}/multi-threaded.js
		-s SHARED_MEMORY)
	set(ENVIRONMENT_FLAG
		-s ENVIRONMENT=web,node,worker
		-s PTHREAD_POOL_SIZE=16)
	set(CMAKE_CXX_FLAGS "-pthread -s SHARED_MEMORY")
	set(OUTPUT_MT_INFIX ".multithread")
else()
	set(MULTI_THREADED_FLAG "")
	set(ENVIRONMENT_FLAG -s ENVIRONMENT=web,node,worker)
	set(OUTPUT_MT_INFIX "")
endif()

# Debug builds get a .debug. infix so Debug/Distribution outputs don't overwrite each other
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(OUTPUT_BASE_NAME "${OUTPUT_FOLDER}jolt-physics.debug${OUTPUT_MT_INFIX}")
else()
	set(OUTPUT_BASE_NAME "${OUTPUT_FOLDER}jolt-physics${OUTPUT_MT_INFIX}")
endif()

# Enable SIMD flags
if (ENABLE_SIMD)
	set(ENABLE_SIMD_FLAG -msimd128 -msse4.2)
	set(USE_WASM_SIMD ON)
else()
	set(ENABLE_SIMD_FLAG "")
	set(USE_WASM_SIMD OFF)
endif()

# Include Jolt
include(FetchContent)
FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY "https://github.com/jrouwe/JoltPhysics"
        GIT_TAG "v5.5.0"
		SOURCE_SUBDIR "Build"
)
FetchContent_MakeAvailable(JoltPhysics)

# Enable resizable heap, with some amount of slowness. Default is 128*1024*1024 = 128MB
if (ALLOW_MEMORY_GROWTH)
	set(MEMORY_GROWTH_FLAG
		-s ALLOW_MEMORY_GROWTH=1
		-s MAXIMUM_MEMORY=${MAXIMUM_MEMORY})
else()
	set(MEMORY_GROWTH_FLAG "")
endif()

# Enable memory profiler
if (ENABLE_MEMORY_PROFILER)
	set(MEMORY_PROFILER_FLAG "--memoryprofiler")
	set(INCLUDE_DEMANGLE_ALL_FLAG "-s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE='$demangleAll'")
else()
	set(MEMORY_PROFILER_FLAG "")
	set(INCLUDE_DEMANGLE_ALL_FLAG "")
endif()

if (USE_CLOSURE)
	set(CLOSURE_FLAG --closure=1)
	set(CLOSURE_IMPORT_ARGS
		"--closure-args=--dynamic_import_alias=replace_by_import"
		"--closure-args=--externs"
		"--closure-args=${CMAKE_CURRENT_SOURCE_DIR}/extern-import.js")
else()
	set(CLOSURE_FLAG "")
	set(CLOSURE_IMPORT_ARGS "")
endif()

# ---- Link (JS-generation) args ----
set(EMCC_ARGS
  -lembind
  -fno-exceptions
  --js-library ${JOLT_NO_FINALIZER_FILE}
  --extern-pre-js ${JOLT_PRE_FILE}
  ${CLOSURE_FLAG}
  ${CLOSURE_IMPORT_ARGS}
  ${MULTI_THREADED_FLAG}
  --post-js ${JOLT_POST_GENERATED}
  ${MEMORY_GROWTH_FLAG}
  -s ALLOW_TABLE_GROWTH=1
  ${ENVIRONMENT_FLAG}
  -s EXPORT_ES6=1
  -s EXPORT_NAME="Jolt"
  -s MODULARIZE=1
  -s NO_EXIT_RUNTIME=1
  -s NO_FILESYSTEM=1
  -s STACK_SIZE=1048576
  -s INITIAL_MEMORY=${INITIAL_MEMORY}
  -s EXPORTED_RUNTIME_METHODS=HEAP8,HEAPU8,HEAP16,HEAPU16,HEAP32,HEAPU32,HEAPF32,HEAPF64,UTF8ToString
  ${MEMORY_PROFILER_FLAG}
  ${INCLUDE_DEMANGLE_ALL_FLAG}
  ${ENABLE_SIMD_FLAG})

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	set(EMCC_ARGS ${EMCC_ARGS} -g${JPH_DEBUG_SYMBOL_FORMAT} -s ASSERTIONS --closure=0)
else()
	set(EMCC_ARGS ${EMCC_ARGS} -O3)
endif()

# Runtime bounds/alignment checking on every heap access (for hunting OOB bugs).
option(SAFE_HEAP "Enable emscripten SAFE_HEAP memory checking" OFF)
if (SAFE_HEAP)
	set(EMCC_ARGS ${EMCC_ARGS} -s SAFE_HEAP=1)
endif()

set(EMCC_JS_ARGS ${EMCC_ARGS}
  -s AGGRESSIVE_VARIABLE_ELIMINATION=1
  -s ELIMINATE_DUPLICATE_FUNCTIONS=1
  -s SINGLE_FILE=1
  -s LEGACY_VM_SUPPORT=1
  -s WASM=0)

# NOTE: -s BINARYEN_IGNORE_IMPLICIT_TRAPS=1 was removed — it assumes memory ops
# never trap and, at -O3, corrupts embind's value_object marshalling (Vec3 write
# -> "memory access out of bounds"). Harmless with the old WebIDL glue; not with embind.
set(EMCC_WASM_ARGS ${EMCC_ARGS}
  -s WASM_BIGINT=1
  -s WASM=1)

set(EMCC_WASM_COMPAT_ARGS ${EMCC_WASM_ARGS}
  -s SINGLE_FILE=1)

project("JoltPhysics.js")

# ---- compile the embind bindings as an OBJECT library linked against Jolt so
#      it inherits ALL of Jolt's PUBLIC compile definitions + include dirs.
#      Compiling by hand risks an ABI/layout mismatch (missing a JPH_* define),
#      which silently corrupts memory at runtime. ----
add_library(jolt_bindings OBJECT ${JOLT_BINDINGS_FILE})
target_link_libraries(jolt_bindings PRIVATE Jolt)
target_compile_features(jolt_bindings PRIVATE cxx_std_17)
target_compile_options(jolt_bindings PRIVATE -Wall -fno-exceptions ${ENABLE_SIMD_FLAG})
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	target_compile_definitions(jolt_bindings PRIVATE JPH_DEBUG_RENDERER JPH_PROFILE_ENABLED _DEBUG)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
	target_compile_definitions(jolt_bindings PRIVATE JPH_DEBUG_RENDERER JPH_PROFILE_ENABLED NDEBUG)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Distribution")
	target_compile_definitions(jolt_bindings PRIVATE NDEBUG)
else()
	message(WARNING "Unknown CMAKE_BUILD_TYPE '${CMAKE_BUILD_TYPE}': no JPH_DEBUG_RENDERER or NDEBUG set")
endif()
set(JOLT_BINDINGS_OBJ $<TARGET_OBJECTS:jolt_bindings>)

# ---- from one --emit-tsd probe link, generate BOTH the shipped dist/types.d.ts and the
#      metadata-baked post.generated.js (facade). gen-bindings.mjs reads the binding-site
#      metadata straight off the probe module (embind val getters — no JSON sidecars). ----
set(JOLT_TYPING_RAW ${CMAKE_CURRENT_BINARY_DIR}/types.raw.d.ts)
set(GEN_BINDINGS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen-bindings.mjs)
add_custom_command(
	OUTPUT ${JOLT_TYPING} ${JOLT_POST_GENERATED}
	COMMAND emcc ${JOLT_BINDINGS_OBJ} $<TARGET_FILE:Jolt> -lembind --emit-tsd ${JOLT_TYPING_RAW}
		-s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME="Jolt" -s WASM_BIGINT=1
		-s ENVIRONMENT=web,node,worker -o ${CMAKE_CURRENT_BINARY_DIR}/types-probe.mjs
	COMMAND node ${GEN_BINDINGS}
		${CMAKE_CURRENT_BINARY_DIR}/types-probe.mjs
		${JOLT_TYPING_RAW} ${JOLT_TYPING}
		${JOLT_POST_FILE} ${JOLT_POST_GENERATED}
	DEPENDS jolt_bindings ${JOLT_BINDINGS_OBJ} ${JOLT_TARGETS} ${GEN_BINDINGS} ${JOLT_POST_FILE}
	COMMENT "Generating JoltPhysics.js TypeScript definitions + facade"
	VERBATIM)
add_custom_target(jolt-typing ALL DEPENDS ${JOLT_TYPING} ${JOLT_POST_GENERATED})

# ---- flavor outputs ----
if (NOT BUILD_WASM_COMPAT_ONLY)
	if (NOT ENABLE_MULTI_THREADING     # Not compatible with multi threading (atomic store not implemented)
		AND NOT ENABLE_SIMD            # Not compatible with SIMD (unhandled unaligned load)
		AND NOT BUILD_WASM_SIDECAR_ONLY)   # sidecar-only build emits just the .wasm sidecar
		add_custom_command(
			OUTPUT ${OUTPUT_BASE_NAME}.js
			COMMAND emcc ${JOLT_BINDINGS_OBJ} $<TARGET_FILE:Jolt> ${EMCC_JS_ARGS} -o ${OUTPUT_BASE_NAME}.js
			DEPENDS jolt_bindings ${JOLT_BINDINGS_OBJ} ${JOLT_PRE_FILE} ${JOLT_POST_GENERATED} ${JOLT_TARGETS} ${JOLT_HEADER_FILE}
			COMMENT "Building JoltPhysics.js javascript"
			VERBATIM)
		add_custom_target(jolt-javascript ALL DEPENDS ${OUTPUT_BASE_NAME}.js)
	endif()

	add_custom_command(
		OUTPUT ${OUTPUT_BASE_NAME}.wasm.js ${OUTPUT_BASE_NAME}.wasm.wasm
		COMMAND emcc ${JOLT_BINDINGS_OBJ} $<TARGET_FILE:Jolt> ${EMCC_WASM_ARGS} -o ${OUTPUT_BASE_NAME}.wasm.js
		DEPENDS jolt_bindings ${JOLT_BINDINGS_OBJ} ${JOLT_PRE_FILE} ${JOLT_POST_GENERATED} ${JOLT_TARGETS} ${JOLT_HEADER_FILE}
		COMMENT "Building JoltPhysics.js webassembly"
		VERBATIM)
	add_custom_target(jolt-wasm ALL DEPENDS ${OUTPUT_BASE_NAME}.wasm.js ${OUTPUT_BASE_NAME}.wasm.wasm)
endif()

# multi-threaded.js references the worker module via a `replace_by_import(...)`
# placeholder so a raw dynamic import() survives the emscripten/closure JS pipeline
# (see JoltPhysics.js issue #245). Swap it back to a real `import` in the final bundle.
# There is exactly one occurrence (the call), so a bare-word replace is safe.
if (ENABLE_MULTI_THREADING)
	set(FIXUP_IMPORT_COMMAND COMMAND perl -i -pe "s/replace_by_import/import/g" ${OUTPUT_BASE_NAME}.wasm-compat.js)
else()
	set(FIXUP_IMPORT_COMMAND "")
endif()

if (NOT BUILD_WASM_SIDECAR_ONLY)
add_custom_command(
	OUTPUT ${OUTPUT_BASE_NAME}.wasm-compat.js
	COMMAND emcc ${JOLT_BINDINGS_OBJ} $<TARGET_FILE:Jolt> ${EMCC_WASM_COMPAT_ARGS} -o ${OUTPUT_BASE_NAME}.wasm-compat.js
	${FIXUP_IMPORT_COMMAND}
	DEPENDS jolt_bindings ${JOLT_BINDINGS_OBJ} ${JOLT_PRE_FILE} ${JOLT_POST_GENERATED} ${JOLT_TARGETS} ${JOLT_HEADER_FILE}
	COMMENT "Building JoltPhysics.js compat webassembly"
	VERBATIM)
add_custom_target(jolt-wasm-compat ALL DEPENDS ${OUTPUT_BASE_NAME}.wasm-compat.js)
endif()
