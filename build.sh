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
	cmake -B Build/$BUILD_TYPE/ST -DCMAKE_BUILD_TYPE=$BUILD_TYPE "${@}"
	cmake --build Build/$BUILD_TYPE/ST -j`nproc`

	cmake -B Build/$BUILD_TYPE/MT -DENABLE_MULTI_THREADING=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE "${@}"
	cmake --build Build/$BUILD_TYPE/MT -j`nproc`

	cmake -B Build/Debug/ST -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_COMPAT_ONLY=ON "${@}"
	cmake --build Build/Debug/ST -j`nproc`

	cmake -B Build/Debug/MT -DENABLE_MULTI_THREADING=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_COMPAT_ONLY=ON "${@}"
	cmake --build Build/Debug/MT -j`nproc`

	# Debuggable debug builds: separate-.wasm sidecars (jolt-physics.debug[.multithread].wasm.js
	# + .wasm.wasm). The base64-embedded compat debug builds OOM bundlers (Vercel) and break
	# Chrome C++ breakpoints, so ship a separate-.wasm sidecar for ST and MT.
	cmake -B Build/Debug/SidecarST -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_SIDECAR_ONLY=ON "${@}"
	cmake --build Build/Debug/SidecarST -j`nproc`

	cmake -B Build/Debug/SidecarMT -DENABLE_MULTI_THREADING=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_SIDECAR_ONLY=ON "${@}"
	cmake --build Build/Debug/SidecarMT -j`nproc`
else
	cmake -B Build/Debug/ST -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_COMPAT_ONLY=ON "${@}"
	cmake --build Build/Debug/ST -j`nproc`

	cmake -B Build/Debug/MT -DENABLE_MULTI_THREADING=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_COMPAT_ONLY=ON "${@}"
	cmake --build Build/Debug/MT -j`nproc`

	# Debuggable debug builds: separate-.wasm sidecars (jolt-physics.debug[.multithread].wasm.js
	# + .wasm.wasm). The base64-embedded compat debug builds OOM bundlers (Vercel) and break
	# Chrome C++ breakpoints, so ship a separate-.wasm sidecar for ST and MT.
	cmake -B Build/Debug/SidecarST -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_SIDECAR_ONLY=ON "${@}"
	cmake --build Build/Debug/SidecarST -j`nproc`

	cmake -B Build/Debug/SidecarMT -DENABLE_MULTI_THREADING=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_SIDECAR_ONLY=ON "${@}"
	cmake --build Build/Debug/SidecarMT -j`nproc`
fi

# Update the worker URL in the debug multithread bundle
if [ -f ./dist/jolt-physics.debug.multithread.wasm-compat.js ]
then
	perl -i -pe "s:jolt-physics.multithread.wasm-compat.js:jolt-physics.debug.multithread.wasm-compat.js:g" ./dist/jolt-physics.debug.multithread.wasm-compat.js
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
	jolt-physics \
	jolt-physics.wasm \
	jolt-physics.wasm-compat \
	jolt-physics.debug.wasm \
	jolt-physics.debug.wasm-compat \
	jolt-physics.multithread \
	jolt-physics.multithread.wasm \
	jolt-physics.multithread.wasm-compat \
	jolt-physics.debug.multithread.wasm \
	jolt-physics.debug.multithread.wasm-compat

cp ./dist/jolt-physics*.wasm-compat.js ./Examples/js/
