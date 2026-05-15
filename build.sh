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

# Preamble: Debug non-compat (ST only) so the npm package ships the .debug.wasm.{js,wasm} pair
# alongside the Release artifacts. JPH_OUTPUT_NAME_SUFFIX=.debug renames CMake's outputs in-place,
# so no mv / sed rewrites are needed afterwards. MT debug is intentionally dropped (no consumers;
# debug-wasm-compat and debug-wasm-compat-multithread used to embed a multi-MB base64 WASM blob in
# JS, which crashed Chrome DevTools when setting C++ breakpoints).
#
# Wipe Build/Debug/ST so a previous run's cached BUILD_WASM_COMPAT_ONLY=ON (from the legacy
# preamble) cannot poison this fresh non-compat configure. We also pass BUILD_WASM_COMPAT_ONLY=OFF
# explicitly as defense-in-depth in case someone passes a non-empty pre-existing Build dir.
#
# Pass ``--verbose`` to ``cmake --build`` (forwards to Ninja ``-v``) only when you need the full
# ``em++`` command line in CI logs (prefix maps, include flags, etc.). It bloats logs by ~20x;
# leave it off for routine publishes.
if [ $BUILD_TYPE != "Debug" ]
then
	rm -rf Build/Debug/ST
	cmake -B Build/Debug/ST -DCMAKE_BUILD_TYPE=Debug -DBUILD_WASM_COMPAT_ONLY=OFF -DJPH_OUTPUT_NAME_SUFFIX=.debug "${@}"
	cmake --build Build/Debug/ST -j`nproc`
fi

cmake -B Build/$BUILD_TYPE/ST -DCMAKE_BUILD_TYPE=$BUILD_TYPE "${@}"
cmake --build Build/$BUILD_TYPE/ST -j`nproc`

cmake -B Build/$BUILD_TYPE/MT -DENABLE_MULTI_THREADING=ON -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE "${@}"
cmake --build Build/$BUILD_TYPE/MT -j`nproc`

cat > ./dist/jolt-physics.d.ts << EOF
import Jolt from "./types";

export default Jolt;
export * from "./types";

EOF

cp ./dist/jolt-physics.d.ts ./dist/jolt-physics.wasm.d.ts
cp ./dist/jolt-physics.d.ts ./dist/jolt-physics.wasm-compat.d.ts
cp ./dist/jolt-physics.d.ts ./dist/jolt-physics.multithread.d.ts
cp ./dist/jolt-physics.d.ts ./dist/jolt-physics.multithread.wasm.d.ts
cp ./dist/jolt-physics.d.ts ./dist/jolt-physics.multithread.wasm-compat.d.ts

if [ $BUILD_TYPE != "Debug" ]
then
	cp ./dist/jolt-physics.d.ts ./dist/jolt-physics.debug.wasm.d.ts
fi

cp ./dist/jolt-physics*.wasm-compat.js ./Examples/js/
