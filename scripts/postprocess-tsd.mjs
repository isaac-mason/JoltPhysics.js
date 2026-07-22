// Turns the raw `--emit-tsd` output into the shipped dist/types.d.ts. Applies the metadata
// sidecars declared at the binding site (out-param / constructor / return types) and the fixes
// emit-tsd can't make itself (module rename, value_array element labels, facade types). Each
// transform fails loud if its anchor is missing, so an emsdk change can't silently ship a bad .d.ts.
//
// Usage: node postprocess-tsd.mjs <raw.d.ts> <out.d.ts> [out-meta.json] [ctor-meta.json] [ret-meta.json]

import { readFileSync, writeFileSync } from 'node:fs';

const [, , inPath, outPath, outMetaPath, ctorMetaPath, retMetaPath] = process.argv;
if (!inPath || !outPath) { console.error('usage: postprocess-tsd.mjs <in> <out> [out-meta] [ctor-meta] [ret-meta]'); process.exit(2); }

const fail = (msg) => { console.error(`postprocess-tsd: ${msg}`); process.exit(1); };
const warn = (msg) => console.warn(`postprocess-tsd: WARNING ${msg}`);
const readJSON = (path, fallback) => (path ? JSON.parse(readFileSync(path, 'utf8')) : fallback);

// embind names the module MainModule/MainModuleFactory; the package exports it as Jolt.
function renameModule(src) {
  if (!/\bMainModule\b/.test(src)) fail('anchor "MainModule" not found — emsdk --emit-tsd output changed');
  return src.replace(/\bMainModuleFactory\b/g, 'JoltFactory').replace(/\bMainModule\b/g, 'JoltModule');
}

// value_array types emit as bare `[number, ...]` (embind has no element-name concept). Add labeled
// tuple elements for hover text. Mat44 (16 elems) is left unlabeled — noise outweighs signal.
const TUPLE_LABELS = {
  Vec3:   '[ x: number, y: number, z: number ]',
  Quat:   '[ x: number, y: number, z: number, w: number ]',
  Vec4:   '[ x: number, y: number, z: number, w: number ]',
  Float3: '[ x: number, y: number, z: number ]',
  Float2: '[ x: number, y: number ]',
  Color:  '[ r: number, g: number, b: number, a: number ]',
  AABox:  '[ minX: number, minY: number, minZ: number, maxX: number, maxY: number, maxZ: number ]',
};
function labelTupleElements(src) {
  for (const [name, labeled] of Object.entries(TUPLE_LABELS)) {
    const re = new RegExp(`export type ${name} = \\[[^\\]]+\\];`);
    if (!re.test(src)) warn(`value_array type ${name} not found`);
    src = src.replace(re, `export type ${name} = ${labeled};`);
  }
  return src;
}

// out_function registers `XInto(out: number, ...): void`; _outMeta gives the real value type.
function applyOutParamTypes(src) {
  const type = {};
  for (const { method, tsTypes } of readJSON(outMetaPath, [])) type[method] = tsTypes?.[0] || 'Vec3';
  if (!/\w+Into\(out: number/.test(src)) warn('no *Into out-param methods found');
  return src.replace(/^(\s*)(\w+)Into\(out: number(, [^)]*)?\): void;/gm, (_, indent, name, rest) => {
    const t = type[name] || 'Vec3';
    return `${indent}${name}(out: ${t}${rest || ''}): ${t};`;
  });
}

// embind can't name constructor params (emits `_0`); _ctorMeta gives each a name and, for `val`
// params, a TS type that overrides emit-tsd's `any`.
function applyCtorParams(src) {
  const spec = readJSON(ctorMetaPath, {});
  const unmapped = new Set();
  src = src.replace(/^(\s*)new\(([^)]*)\): (\w+);/gm, (line, indent, params, cls) => {
    const ps = spec[cls];
    if (!ps) { if (params.includes('_')) unmapped.add(cls); return line; }
    const renamed = params.split(', ').map((p, i) => {
      const s = ps[i];
      if (!s) return p;
      return `${s.n ?? `arg${i}`}: ${s.t ?? (p.match(/:\s*(.+)$/)?.[1] ?? 'any')}`;
    }).join(', ');
    return `${indent}new(${renamed}): ${cls};`;
  });
  if (unmapped.size) warn(`unmapped constructor params for: ${[...unmapped].join(', ')} (add named constructor in bindings.cpp)`);
  return src;
}

// A lambda returning emscripten::val emits as `any`; _retMeta gives the real type per (class,
// method). The rewrite is scoped to the declaring interface and matches any param list.
function applyReturnTypes(src) {
  const meta = readJSON(retMetaPath, []);
  const byClass = {};
  for (const { cls, method, tsType } of meta) (byClass[cls] ??= {})[method] = tsType;
  const seen = new Set();
  let cls = null;
  src = src.split('\n').map((line) => {
    const decl = line.match(/^export (?:interface|class) (\w+)/);
    if (decl) { cls = decl[1]; return line; }
    const m = cls && byClass[cls] && line.match(/^(\s*)(\w+)\((.*)\): any;$/);
    if (m && byClass[cls][m[2]]) { seen.add(`${cls}.${m[2]}`); return `${m[1]}${m[2]}(${m[3]}): ${byClass[cls][m[2]]};`; }
    return line;
  }).join('\n');
  for (const { cls, method } of meta)
    if (!seen.has(`${cls}.${method}`)) warn(`return-type override for ${cls}.${method} matched no "): any;" line`);
  return src;
}

// embind's synthesized allow_subclass helpers emit unnamed `_0` params.
function nameWrapperParams(src) {
  return src.replace(/\bimplement\(_0: any\)/g, 'implement(obj: any)')
           .replace(/\bextend\(_0: (EmbindString), _1: any\)/g, 'extend(name: $1, obj: any)');
}

// facade.js is hand-written JS with no embind binding, so its types are declared here and merged
// into the module type.
const FACADE_TYPES = `
export type Contact = {
  body1: number; body2: number; subShape1: number; subShape2: number;
  normal: Vec3; penetration: number; isNew: boolean; pointCount: number;
};
export type ContactPoint = { on1: Vec3; on2: Vec3 };
export type RemovedContact = { body1: number; subShape1: number; body2: number; subShape2: number };
/** Buffered contact events — zero-allocation bulk reads (see ContactListenerBuffer). */
export interface ContactBuffer {
  readonly contactCount: number;
  readonly removedCount: number;
  clear(): void;    // before Step
  refresh(): void;  // after Step
  getContact(out: Contact, index: number): Contact;
  getPoint(out: ContactPoint, contact: Contact, pointIndex: number): ContactPoint;
  getRemoved(out: RemovedContact, index: number): RemovedContact;
  destroy(): void;
}
export type ActiveBodyState = {
  id: number;
  position: [number, number, number];
  rotation: [number, number, number, number];
  linVel: [number, number, number];
  angVel: [number, number, number];
};
/** Bulk active-body state — one refresh packs all active bodies' pose+velocity into the WASM heap for zero-crossing reads (see ActiveBodyBuffer). */
export interface ActiveBodyBufferHandle {
  readonly bodyCount: number;
}
export interface JoltFacade {
  createContactBuffer(physicsSystem: PhysicsSystem): ContactBuffer;
  createContact(): Contact;
  createContactPoint(): ContactPoint;
  createRemovedContact(): RemovedContact;
  createActiveBodyBuffer(physicsSystem: PhysicsSystem): ActiveBodyBufferHandle;
  updateActiveBodyBuffer(buffer: ActiveBodyBufferHandle): void;
  getActiveBodyBufferStateAt(buffer: ActiveBodyBufferHandle, out: ActiveBodyState, index: number): ActiveBodyState;
  createActiveBodyState(): ActiveBodyState;
  destroyActiveBodyBuffer(buffer: ActiveBodyBufferHandle): void;
}
`;
function injectFacadeTypes(src) {
  if (!/export type JoltModule = /.test(src)) fail('anchor "export type JoltModule" not found');
  return src.replace(/export type JoltModule = ([^;]+);/, FACADE_TYPES + '\nexport type JoltModule = $1 & JoltFacade;');
}

function setBanner(src) {
  return src.replace(/^\/\/ TypeScript bindings.*$/m,
    '// TypeScript definitions for JoltPhysics.js. Auto-generated (embind --emit-tsd +\n' +
    '// scripts/postprocess-tsd.mjs). Do not edit by hand.');
}

let src = readFileSync(inPath, 'utf8');
for (const transform of [renameModule, labelTupleElements, applyOutParamTypes, applyCtorParams,
                         applyReturnTypes, nameWrapperParams, injectFacadeTypes, setBanner])
  src = transform(src);
writeFileSync(outPath, src);
console.log(`postprocess-tsd: wrote ${outPath}`);
