#!/bin/sh
set -e

if [ -z $1 ]
then
	BUILD_TYPE=Distribution
else
	BUILD_TYPE=$1
	shift
fi

rm -rf ./dist
mkdir dist

# Build order: Distribution first, Debug last. The Debug build's types.d.ts includes
# the debug renderer types and is the most complete, so it wins the final types.d.ts.

if [ $BUILD_TYPE != "Debug" ]
then
	cmake -B Build/$BUILD_TYPE/ST -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE "${@}"
	cmake --build Build/$BUILD_TYPE/ST -j`nproc`

	# Multi-threaded release flavour. Not used by Nilo (Jolt's MT build doesn't work with JS
	# callbacks) but Examples/{conveyor_belt,stress_test}_threaded.html import
	# dist/jolt-physics.multithread.wasm-compat.js, so the demos need it built. Debug MT is
	# deliberately NOT built — nothing imports it and it cost ~30 MB per build.
	cmake -B Build/$BUILD_TYPE/MT -DENABLE_MULTI_THREADING=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE "${@}"
	cmake --build Build/$BUILD_TYPE/MT -j`nproc`

	cmake -B Build/Debug/ST -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_COMPAT_ONLY=ON "${@}"
	cmake --build Build/Debug/ST -j`nproc`

	# Debuggable debug build: separate-.wasm sidecar (jolt-physics.debug.wasm.js + .wasm.wasm) with
	# full DWARF, so Chrome can set C++ breakpoints. The base64-embedded compat debug build OOMs
	# bundlers (Vercel), so the sidecar form is what Nilo loads via `jolt-physics/debug-wasm`.
	cmake -B Build/Debug/SidecarST -DJPH_FULL_DWARF=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_SIDECAR_ONLY=ON "${@}"
	cmake --build Build/Debug/SidecarST -j`nproc`
else
	cmake -B Build/Debug/ST -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_COMPAT_ONLY=ON "${@}"
	cmake --build Build/Debug/ST -j`nproc`

	# Debuggable debug build: separate-.wasm sidecar (jolt-physics.debug.wasm.js + .wasm.wasm) with
	# full DWARF, so Chrome can set C++ breakpoints. The base64-embedded compat debug build OOMs
	# bundlers (Vercel), so the sidecar form is what Nilo loads via `jolt-physics/debug-wasm`.
	cmake -B Build/Debug/SidecarST -DJPH_FULL_DWARF=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_SIDECAR_ONLY=ON "${@}"
	cmake --build Build/Debug/SidecarST -j`nproc`
fi

# Per-flavor .d.ts wrappers — all reference the single types.d.ts (which contains
# the most complete type set, including debug renderer, from the Debug build).
make_dts() {
	for flavor in "$@"; do
		cat > "./dist/${flavor}.d.ts" << DTSEOF
import Jolt from "./types";

export default Jolt;
export * from "./types";

DTSEOF
	done
}

make_dts \
	jolt-physics.wasm \
	jolt-physics.wasm-compat \
	jolt-physics.debug.wasm \
	jolt-physics.debug.wasm-compat \
	jolt-physics.multithread.wasm \
	jolt-physics.multithread.wasm-compat

cp ./dist/jolt-physics*.wasm-compat.js ./Examples/js/
