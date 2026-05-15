**Distribution:** npm-installable `.tgz` attached to each [GitHub Release](https://github.com/nilo-technologies/JoltPhysics.js/releases) (no registry, no auth — public-repo release assets are anonymous-readable).

[![Build Status](https://github.com/nilo-technologies/JoltPhysics.js/actions/workflows/nilo-build-and-publish.yml/badge.svg)](https://github.com/nilo-technologies/JoltPhysics.js/actions/workflows/nilo-build-and-publish.yml)

# Nilo fork

Private fork of `JoltPhysics.js`. We don't merge back upstream; this section is the operating manual. Upstream README preserved [below](#joltphysicsjs-upstream-readme) for API reference.

## Setup

Clone these as **siblings** under one parent (e.g. `C:\dev`); build scripts auto-detect the layout (otherwise set `JOLT_PHYSICS_PATH` and `EMSDK`). Requires CMake >= 3.20, Ninja, Node >= 18.

| Folder | Repo | Notes |
|---|---|---|
| `JoltPhysics.js/` (this repo) | `nilo-technologies/JoltPhysics.js` | branch `nilo` |
| `JoltPhysics/` | `nilo-technologies/JoltPhysics` | C++ Jolt fork; tag `nilo-v5.5.0` |
| `emsdk/` | `emscripten-core/emsdk` | `emsdk install latest && emsdk activate latest` |
| `Nilo/` | _private_ | the consumer |

Both Jolt repos use paired `nilo-vX.Y.Z` tags where `X.Y.Z` matches the upstream Jolt C++ API line. Build scripts default to `nilo-v5.5.0`; override with `$env:NILO_JOLT_TAG`.

## Fast C++ iteration

Edit C++ in `C:\dev\JoltPhysics`, rebuild locally, run Nilo against your build without republishing:

```powershell
just jolt-iter                     # Debug, ~30-60 s/iter (closure on)
just jolt-iter "-FastLink"         # Debug, ~5-20 s/iter (closure off, DevTools-safe)
just jolt-iter "-Variant Release"  # publish-format wasm-compat
```

(Or `.\iter-build-windows.ps1 [-FastLink] [-Variant Release]` from this repo.) Reuses one Ninja build dir at `Build\Iter\<Variant>\ST` and emits to `dist\`:

| Variant | Output | Used for |
|---|---|---|
| Debug | `jolt-physics.wasm.js` + `jolt-physics.wasm.wasm` (non-compat sidecar) | C++ debugging in DevTools |
| Release | `jolt-physics.wasm-compat.js` (single-file) | publish-format parity |

Debug uses non-compat because the wasm-compat single-file embeds ~30 MB of DWARF-laden WASM as base64, producing a ~42 MB JS source that crashes DevTools on C++ breakpoints. Non-compat exposes the WASM as a binary asset DevTools handles natively.

### Wire iter into Nilo

Add to `C:\dev\Nilo\.env.local`:

```env
NILO_JOLT_LOCAL_DIST=C:/dev/JoltPhysics.js/dist
NILO_JOLT_DEBUG=true    # must match the variant of your last iter build
```

Then `pnpm dev` from `C:\dev\Nilo`. Vite re-aliases `jolt-physics`, disables `optimizeDeps` for it, and extends `server.fs.allow` to serve the sidecar. Subsequent iter rebuilds need only a hard-refresh — no Vite restart.

When `NILO_JOLT_LOCAL_DIST` is set, Nilo's Dev Menu → Physics "Debug" toggle auto-hides. Without it (published-package flow), flipping the toggle dynamic-imports `jolt-physics/debug-wasm` from `node_modules` so debug bytes only download on demand.

### C++ debugging in Chrome DevTools

Install the **C/C++ DevTools Support (DWARF)** extension. In its options → **Path substitutions**, add (more-specific first):

| Source path (in DWARF) | Local path |
|---|---|
| `JoltPhysics.js/` | `C:\dev\JoltPhysics.js\` |
| `JoltPhysics/`    | `C:\dev\JoltPhysics\` |

These work for both local-iter builds and the published debug package — `CMakeLists.txt` writes portable DWARF paths via `-fdebug-prefix-map` / `-fmacro-prefix-map` / `-fdebug-compilation-dir=.`.

## Publishing

CI-only; don't pack a tarball from a workstation.

1. Bump `package.json` version. Scheme is `MAJOR.MINOR.PATCH-nilo.N` (npm only allows three numeric segments; the fork counter lives in the prerelease tag). `MAJOR.MINOR.PATCH` mirrors the upstream Jolt C++ API line; `nilo.N` is our fork counter (CI / packaging / binding tweaks):
   ```powershell
   npm version prerelease --preid=nilo --no-git-tag-version  # 5.5.0-nilo.N -> 5.5.0-nilo.(N+1)
   npm version 5.6.0-nilo.0 --no-git-tag-version             # new Jolt C++ API pin
   ```
2. Push paired `nilo-vX.Y.Z` tags to `nilo-technologies/JoltPhysics` first, then this repo.
3. **Actions → Nilo Build and Publish → Run workflow.** Leave **dry run** on for a `jolt-physics-dist` build artifact only; turn it off to create a GitHub Release `v<version>` with both `nilo-technologies-jolt-physics-<version>.tgz` (npm-installable) and the `dist/` tarball (debug provenance) attached. The release step is idempotent — a no-op rebuild on the same version skips creating a duplicate release.

## Consume from Nilo

After publishing, bump the pin in two places — `Nilo/package.json` and `Nilo/packages/physics-with-jolt/package.json`. From the Nilo repo:

```powershell
just bump-jolt 5.5.0-nilo.N
```

This rewrites both pins to `https://github.com/nilo-technologies/JoltPhysics.js/releases/download/v5.5.0-nilo.N/nilo-technologies-jolt-physics-5.5.0-nilo.N.tgz` and runs `pnpm install` to refresh the lockfile.

# JoltPhysics.js (upstream README)

This project enables using [Jolt Physics](https://github.com/nilo-technologies/JoltPhysics) (Nilo's C++ fork) in JavaScript.

## Demos

Go to the [demos page](https://jrouwe.github.io/JoltPhysics.js/) to see the project in action.

## Using

This library comes in 6 flavours:
- `wasm-compat` - A WASM version with the WASM file (encoded in base64) embedded in the bundle
- `wasm` - A WASM version with a separate WASM file
- `debug-wasm` - Same as `wasm` but compiled with DWARF + assertions for C++ source-level debugging (see Nilo-fork section above for why this replaces upstream's `debug-wasm-compat`).
- `asm` - A JavaScript version that uses [asm.js](https://developer.mozilla.org/en-US/docs/Games/Tools/asm.js)
- `wasm-compat-multithread` - Same as `wasm-compat` but with multi threading enabled.
- `wasm-multithread` - Same as `wasm` but with multi threading enabled.

See [falling_shapes.html](Examples/falling_shapes.html) for an example on how to use the library.

### Documentation

The interface of the library is the same as the C++ interface of JoltPhysics, this means that you can use the [C++ documentation](https://jrouwe.github.io/JoltPhysics/) as reference.

Almost the entire Jolt interface has been exposed. Check [JoltJS.idl](https://github.com/jrouwe/JoltPhysics.js/blob/main/JoltJS.idl) if a particular interface has been exposed. If not, edit [JoltJS.idl](https://github.com/jrouwe/JoltPhysics.js/blob/main/JoltJS.idl) and [JoltJS.h](https://github.com/jrouwe/JoltPhysics.js/blob/main/JoltJS.h) and send a pull request, or open an issue.

### Installation

Pin the GitHub Release tarball directly in `package.json`:

```json
"jolt-physics": "https://github.com/nilo-technologies/JoltPhysics.js/releases/download/v5.5.0-nilo.10/nilo-technologies-jolt-physics-5.5.0-nilo.10.tgz"
```

(Substitute the latest version from [Releases](https://github.com/nilo-technologies/JoltPhysics.js/releases).) `pnpm install` / `npm install` fetches the tarball and pins its integrity hash in the lockfile. No registry or auth involved. The dep-key (`jolt-physics` here) becomes the import name; pick whatever suits your codebase, but `jolt-physics` matches upstream's docs.

The different flavours are available via entrypoints on the package:

```js
// WASM embedded in the bundle
import Jolt from 'jolt-physics';
import Jolt from 'jolt-physics/wasm-compat';

// WASM
import Jolt from 'jolt-physics/wasm';

// WASM with DWARF + assertions for C++ source-level debugging (separate .wasm sidecar)
import Jolt from 'jolt-physics/debug-wasm';

// asm.js
import Jolt from 'jolt-physics/asm';

// WASM embedded in the bundle, multithread enabled
import Jolt from 'jolt-physics/wasm-compat-multithread';

// WASM, multithread enabled
import Jolt from 'jolt-physics/wasm-multithread';
```

### Using the WASM flavour

To use the `wasm` flavour, you must either serve the WASM file `jolt-physics.wasm.wasm` alongside `jolt-physics.wasm.js`, or use a bundler that supports importing an asset as a url, and tell Jolt where to find the WASM file.

To specify where to retrieve the WASM file from, you can pass a `locateFile` function to the default export of `jolt-physics/wasm`. For example, using [vite](https://vitejs.dev/) this would look like:

```js
import initJolt from "jolt-physics";
import joltWasmUrl from "jolt-physics/jolt-physics.wasm.wasm?url";

const Jolt = await initJolt({
  locateFile: () => joltWasmUrl,
});
```

For more information on the `locateFile` function, see the [Emscripten documentation](https://emscripten.org/docs/api_reference/module.html#Module.locateFile).

## Building

This project has only been compiled under Linux.

* Install [emscripten](https://emscripten.org/) and ensure that its environment variables have been setup
* Install [cmake](https://cmake.org/)
* Run ```./build.sh``` to build both the Debug and Distribution build, ```./build.sh Debug``` for only the Debug build.

Additional options that can be provided to ```build.sh```:

* ```-DENABLE_MEMORY_PROFILER=ON``` will enable memory tracking to detect leaks.
* ```-DDOUBLE_PRECISION=ON``` will enable the double precision mode. This allows worlds larger than a couple of km.
* ```-DENABLE_SIMD=ON``` will enable SIMD instructions. Safari 16.4 was the last major browser to support this (in March 2023). The multithreaded builds have this on by default.
* ```-DBUILD_WASM_COMPAT_ONLY=ON``` speeds up the build by only compiling the WASM compat version which the examples use.
* ```-DCROSS_PLATFORM_DETERMINISTIC=ON``` builds the library so that it produces the same results as the native version of the library. For more info [click here](https://jrouwe.github.io/JoltPhysics/#deterministic-simulation).

## Running

By default the examples use the WASM compat version of Jolt. This requires serving the html file using a web server rather than opening the html file directly.

Open a terminal in this folder and run the following commands:

```
npm install
npm run examples
```

Then navigate to: [http://localhost:3000/](http://localhost:3000/)

If you need to debug the C++ code take a look at [WASM debugging](https://developer.chrome.com/blog/wasm-debugging-2020/).

## Memory Management

The samples are very bad at cleaning up after themselves (basically they don't). When using emscripten to port a library to WASM, [nothing is cleaned up](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/WebIDL-Binder.html#using-c-classes-in-javascript) automatically, so everything you newed with ```new Jolt.XXX``` needs to be destroyed by ```Jolt.destroy(...)```.

On top of this, Jolt uses reference counting for a number of its classes (everything that inherits from [RefTarget](https://jrouwe.github.io/JoltPhysics/class_ref_target.html)). The most important classes are:

* ShapeSettings
* Shape
* ConstraintSettings
* Constraint
* PathConstraintPath
* PhysicsMaterial
* GroupFilter
* SoftBodySharedSettings
* VehicleCollisionTester
* VehicleControllerSettings
* WheelSettings
* CharacterBaseSettings
* CharacterBase
* Skeleton
* SkeletonAnimation
* SkeletonMapper
* PhysicsScene
* RagdollSettings
* Ragdoll

Reference counting objects start with a reference count of 0. If you want to keep ownership over the object, you need to call ```object.AddRef()```, this will increment the reference count. If you want to release ownership you call ```object.Release()```, this will decrement the reference count and if the reference count reaches 0 the object will be destroyed. If, after newing, you pass a reference counted object on to another object (e.g. a ShapeSettings to a CompoundShapeSettings or a Shape to a Body) then that other object will take a reference, in that case it is not needed take a reference yourself beforehand so you can skip the calls to ```AddRef/Release```. Note that it is also possible to do ```new Jolt.XXX``` followed by ```Jolt.destroy(...)``` for a reference counted object if no one took a reference.

The Body class is also a special case, it is destroyed through BodyInterface.DestroyBody(body.GetID()) (which internally destroys the Body).

Almost everything else can be destroyed straight after it has been passed to Jolt. [An example that shows how to properly clean up using Jolt is here](https://github.com/jrouwe/JoltPhysics.js/blob/main/Examples/proper_cleanup.html).

## Projects using JoltPhysics.js

* [Babylon.js plugin](https://github.com/PhoenixIllusion/babylonjs-jolt-physics-plugin) - A plugin that replaces the default physics engine with Jolt.
* [GDevelop](https://gdevelop.io/) - An Open-source, cross-platform 2D/3D/multiplayer game engine. See [announcement](https://github.com/4ian/GDevelop/releases/tag/v5.5.220).
* [react-three-jolt](https://github.com/pmndrs/react-three-jolt) - Wraps Jolt to make it easy to use in react-three-fiber.
* [r3f-jolt](https://github.com/sajal353/r3f-jolt) - Another wrapper for react-three-fiber.
* [Synthesis](https://github.com/Autodesk/synthesis) - A Robotics Simulator for Autodesk Fusion CAD Designs

## License

The project is distributed under the [MIT license](LICENSE).
