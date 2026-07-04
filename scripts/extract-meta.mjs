// Runs the --emit-tsd probe binary and dumps its binding-site metadata getters to JSON sidecars
// for postprocess-tsd.mjs. Called by CMake after the emit-tsd link step.
// Usage: node extract-meta.mjs <probe.mjs> <out-meta.json> <ctor-meta.json> <ret-meta.json>

import { writeFileSync } from 'node:fs';

const [, , probePath, ...outPaths] = process.argv;
const sidecars = [['_outMeta', outPaths[0]], ['_ctorMeta', outPaths[1]], ['_retMeta', outPaths[2]]];
if (!probePath || sidecars.some(([, path]) => !path)) {
  console.error('usage: extract-meta.mjs <probe.mjs> <out-meta.json> <ctor-meta.json> <ret-meta.json>');
  process.exit(2);
}

let jolt;
try {
  jolt = await (await import(probePath)).default();
} catch (e) {
  console.error(`extract-meta: failed to load probe binary "${probePath}":`, e.message ?? e);
  process.exit(1);
}

try {
  for (const [getter, path] of sidecars) writeFileSync(path, jolt[getter]());
} catch (e) {
  console.error('extract-meta: failed to read metadata from probe binary:', e.message ?? e);
  process.exit(1);
}
console.log(`extract-meta: wrote ${sidecars.map(([, path]) => path).join(', ')}`);
