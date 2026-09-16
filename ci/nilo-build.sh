#!/bin/bash
# Nilo fork build entry point. Mirrors upstream `ci/build-examples.sh` but:
#   - `set -e` so any failed step aborts the workflow (upstream does not).
#   - Branches on $NILO_CI_JOLT_PHYSICS_PATH so CI builds against the sibling-cloned
#     Nilo C++ JoltPhysics fork (set in `.github/workflows/nilo-build-and-publish.yml`)
#     instead of the upstream FetchContent default.
#
# Kept as a separate script (rather than a modification to `ci/build-examples.sh`) so
# upstream merges into this fork are conflict-free.

set -e

npm install

bash ./ci/install-emsdk.sh
source ./emsdk/emsdk_env.sh

if [ -n "${NILO_CI_JOLT_PHYSICS_PATH:-}" ] && [ -f "${NILO_CI_JOLT_PHYSICS_PATH}/Build/CMakeLists.txt" ]; then
  echo "Building with local Jolt C++ tree: ${NILO_CI_JOLT_PHYSICS_PATH}"
  sh ./build.sh Distribution "-DJOLT_PHYSICS_PATH=${NILO_CI_JOLT_PHYSICS_PATH}"
else
  echo "NILO_CI_JOLT_PHYSICS_PATH not set or invalid; falling back to FetchContent (upstream default)."
  npm run build
fi
