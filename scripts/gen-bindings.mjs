// Single source of the generated bindings artifacts, driven by the binding-site metadata that
// bindings.cpp exposes via embind `val` getters (_outMeta/_ctorMeta/_retMeta/_layoutMeta). Reads
// them straight off the --emit-tsd probe module in-process — no JSON strings, no sidecar files —
// then produces both:
//   1. dist/types.d.ts   — the raw --emit-tsd output with the fixes emit-tsd can't make itself
//                          (module rename, value_array element labels, out-param/ctor/return types
//                          from the binding site, facade types), internal getters stripped.
//   2. post.generated.js — post.js with its __JOLT_OUT_META__ / __JOLT_LAYOUT__ tokens replaced by
//                          baked literals, so the shipped runtime never decodes a getter's return.
// Each .d.ts transform fails loud if its anchor is missing, so an emsdk change can't silently ship
// a bad .d.ts.
//
// Usage: node gen-bindings.mjs <probe.mjs> <raw.d.ts> <out.d.ts> <post-template.js> <post-generated.js>

import { readFileSync, writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const [, , probePath, rawTsdPath, outTsdPath, postTemplatePath, postOutPath] = process.argv;
if (!probePath || !rawTsdPath || !outTsdPath || !postTemplatePath || !postOutPath) {
  console.error('usage: gen-bindings.mjs <probe.mjs> <raw.d.ts> <out.d.ts> <post-template.js> <post-generated.js>');
  process.exit(2);
}

const fail = (msg) => { console.error(`gen-bindings: ${msg}`); process.exit(1); };
const warn = (msg) => console.warn(`gen-bindings: WARNING ${msg}`);

// ---- 1. read binding-site metadata off the emit-tsd probe (native JS objects via embind val) ----
let outMeta, ctorMeta, retMeta, layoutMeta;
try {
  const jolt = await (await import(pathToFileURL(probePath).href)).default();
  outMeta = jolt._outMeta();     // [{cls, method, names:[…], sizes:[…], outTs:[…], passKinds:[…], passTs:[…]}]
  ctorMeta = jolt._ctorMeta();   // {ClassName: [{n, t?}, …]}
  retMeta = jolt._retMeta();     // [{cls, method, tsType}]
  layoutMeta = jolt._layoutMeta(); // {contactI32, contactF32, pointF32, removedI32, activeBody}
} catch (e) {
  fail(`failed to load probe binary "${probePath}": ${e?.message ?? e}`);
}
if (!Array.isArray(outMeta) || !outMeta.length) fail('_outMeta() returned no entries — did the out_function DSL change?');

// ---- 2. postprocess the raw .d.ts ----

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

// out_function registers a raw `MethodInto(a0, a1, ...): void` (loose scalars — the facade unpacks
// vecs/quats/mats before the wasm crossing). Replace each with the public reader signature rebuilt
// from the binding-site metadata: `names` (public params, out slots then passes), `outTs` (out value
// types) and `passTs` (input types). A multi-out getter returns a tuple of its out types. Scoped to
// the declaring interface — same-named methods on different classes (GetPointVelocity on Body vs
// BodyInterface) have different signatures, so a method-name-only map would cross them.
function applyOutParamTypes(src) {
  const byClass = {};
  for (const { cls, method, names, outTs, passTs, retTs } of outMeta)
    (byClass[cls] ??= {})[method] = { names, outTs, passTs, retTs };
  if (!/\w+Into\(/.test(src)) warn('no *Into out-param methods found');
  const seen = new Set();
  let cls = null;
  src = src.split('\n').map((line) => {
    const decl = line.match(/^export (?:interface|class) (\w+)/);
    if (decl) { cls = decl[1]; return line; }
    // A value-returning out_function renders `...Into(...): T`, not `: void` — match any return.
    const m = cls && byClass[cls] && line.match(/^(\s*)(\w+)Into\([^)]*\): [^;]+;\s*$/);
    if (m && byClass[cls][m[2]]) {
      const { names, outTs, passTs, retTs } = byClass[cls][m[2]];
      seen.add(`${cls}.${m[2]}`);
      const paramTypes = [...outTs, ...passTs]; // positional: out slots then passes
      const params = names.map((n, i) => `${n}: ${paramTypes[i]}`).join(', ');
      // retTs (value-returning) wins; else single out returns itself, multi returns a tuple.
      const ret = retTs || (outTs.length === 1 ? outTs[0] : `[${outTs.join(', ')}]`);
      return `${m[1]}${m[2]}(${params}): ${ret};`;
    }
    return line;
  }).join('\n');
  for (const { cls, method } of outMeta)
    if (!seen.has(`${cls}.${method}`)) warn(`out-param retype for ${cls}.${method} matched no "${method}Into(...): void;" line`);
  return src;
}

// embind can't name constructor params (emits `_0`); _ctorMeta gives each a name and, for `val`
// params, a TS type that overrides emit-tsd's `any`.
function applyCtorParams(src) {
  const spec = ctorMeta;
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
  const byClass = {};
  for (const { cls, method, tsType } of retMeta) (byClass[cls] ??= {})[method] = tsType;
  const seen = new Set();
  let cls = null;
  src = src.split('\n').map((line) => {
    const decl = line.match(/^export (?:interface|class) (\w+)/);
    if (decl) { cls = decl[1]; return line; }
    const m = cls && byClass[cls] && line.match(/^(\s*)(\w+)\((.*)\): any;$/);
    if (m && byClass[cls][m[2]]) { seen.add(`${cls}.${m[2]}`); return `${m[1]}${m[2]}(${m[3]}): ${byClass[cls][m[2]]};`; }
    return line;
  }).join('\n');
  for (const { cls, method } of retMeta)
    if (!seen.has(`${cls}.${method}`)) warn(`return-type override for ${cls}.${method} matched no "): any;" line`);
  return src;
}

// embind's synthesized allow_subclass helpers emit unnamed `_0` params.
function nameWrapperParams(src) {
  return src.replace(/\bimplement\(_0: any\)/g, 'implement(obj: any)')
           .replace(/\bextend\(_0: (EmbindString), _1: any\)/g, 'extend(name: $1, obj: any)');
}

// The metadata getters + out-param scratch pointer are internal plumbing (read by the build /
// facade), not public API — strip them from the shipped types. With `val` return they emit as
// `any`; _getOutScratch is a `number`.
function stripInternal(src) {
  for (const name of ['_getOutScratch', '_outMeta', '_ctorMeta', '_retMeta', '_layoutMeta']) {
    const re = new RegExp(`^\\s*${name}\\(\\): (?:any|number);\\r?\\n`, 'm');
    if (!re.test(src)) warn(`internal getter ${name} not found to strip`);
    src = src.replace(re, '');
  }
  return src;
}

// post.js is hand-written JS with no embind binding, so its types are declared here and merged
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

// The CharacterContactListener callbacks can't mutate their Vec3& out-params from JS (Vec3 is a
// value_array — passed by copy), so a callback overrides an output by RETURNING an object naming
// what to change. These types document that contract, which embind's `implement(obj: any)` hides.
const CHARACTER_LISTENER_TYPES = `
/** Return from a *ContactSolve callback to override the resolved character velocity; omit (or return nothing) to keep Jolt's. */
export type CharacterVelocityOverride = { velocity?: Vec3 };
/** Return from OnAdjustBodyVelocity to override the contacting body's velocity; omit a field to keep it. */
export type AdjustedBodyVelocity = { linear?: Vec3; angular?: Vec3 };
/** Shape of the object passed to \`CharacterContactListener.implement(...)\`. Every callback is optional.
 * bodyID2 / subShapeID2 / otherCharacterID are numeric ids; ioSettings is mutated in place (a real
 * handle), whereas velocity overrides are returned (see CharacterVelocityOverride / AdjustedBodyVelocity). */
export interface CharacterContactListenerCallbacks {
  OnContactValidate?(character: CharacterVirtual, bodyID2: number, subShapeID2: number): boolean;
  OnContactAdded?(character: CharacterVirtual, bodyID2: number, subShapeID2: number, contactPosition: Vec3, contactNormal: Vec3, ioSettings: CharacterContactSettings): void;
  OnContactPersisted?(character: CharacterVirtual, bodyID2: number, subShapeID2: number, contactPosition: Vec3, contactNormal: Vec3, ioSettings: CharacterContactSettings): void;
  OnContactRemoved?(character: CharacterVirtual, bodyID2: number, subShapeID2: number): void;
  OnAdjustBodyVelocity?(character: CharacterVirtual, body2: Body, linearVelocity: Vec3, angularVelocity: Vec3): AdjustedBodyVelocity | void;
  OnContactSolve?(character: CharacterVirtual, bodyID2: number, subShapeID2: number, contactPosition: Vec3, contactNormal: Vec3, contactVelocity: Vec3, characterVelocity: Vec3, newCharacterVelocity: Vec3): CharacterVelocityOverride | void;
  OnCharacterContactValidate?(character: CharacterVirtual, otherCharacter: CharacterVirtual, subShapeID2: number): boolean;
  OnCharacterContactAdded?(character: CharacterVirtual, otherCharacter: CharacterVirtual, subShapeID2: number, contactPosition: Vec3, contactNormal: Vec3, ioSettings: CharacterContactSettings): void;
  OnCharacterContactPersisted?(character: CharacterVirtual, otherCharacter: CharacterVirtual, subShapeID2: number, contactPosition: Vec3, contactNormal: Vec3, ioSettings: CharacterContactSettings): void;
  OnCharacterContactRemoved?(character: CharacterVirtual, otherCharacterID: number, subShapeID2: number): void;
  OnCharacterContactSolve?(character: CharacterVirtual, otherCharacter: CharacterVirtual, subShapeID2: number, contactPosition: Vec3, contactNormal: Vec3, contactVelocity: Vec3, characterVelocity: Vec3, newCharacterVelocity: Vec3): CharacterVelocityOverride | void;
}
`;
// Emit the callback interface and point CharacterContactListener.implement at it (embind types it as `any`).
function typeCharacterListener(src) {
  const anchor = 'implement(obj: any): CharacterContactListenerWrapper;';
  if (!src.includes(anchor)) fail('anchor for CharacterContactListener.implement not found');
  src = src.replace(anchor, 'implement(obj: CharacterContactListenerCallbacks): CharacterContactListenerWrapper;');
  if (!/export type JoltModule = /.test(src)) fail('anchor "export type JoltModule" not found (character listener types)');
  return src.replace(/export type JoltModule = /, CHARACTER_LISTENER_TYPES + '\nexport type JoltModule = ');
}
function injectFacadeTypes(src) {
  if (!/export type JoltModule = /.test(src)) fail('anchor "export type JoltModule" not found');
  return src.replace(/export type JoltModule = ([^;]+);/, FACADE_TYPES + '\nexport type JoltModule = $1 & JoltFacade;');
}

function setBanner(src) {
  return src.replace(/^\/\/ TypeScript bindings.*$/m,
    '// TypeScript definitions for JoltPhysics.js. Auto-generated (embind --emit-tsd +\n' +
    '// scripts/gen-bindings.mjs). Do not edit by hand.');
}

let src = readFileSync(rawTsdPath, 'utf8');
for (const transform of [renameModule, labelTupleElements, applyOutParamTypes, applyCtorParams,
                         applyReturnTypes, nameWrapperParams, typeCharacterListener, stripInternal,
                         injectFacadeTypes, setBanner])
  src = transform(src);
writeFileSync(outTsdPath, src);
console.log(`gen-bindings: wrote ${outTsdPath}`);

// ---- 3. codegen the facade post-js: bake the metadata literals into the template ----
const postTemplate = readFileSync(postTemplatePath, 'utf8');
for (const tok of ['__JOLT_OUT_META__', '__JOLT_LAYOUT__'])
  if (!postTemplate.includes(tok)) fail(`token ${tok} missing from ${postTemplatePath}`);
const postGen = postTemplate
  .replaceAll('__JOLT_OUT_META__', JSON.stringify(outMeta))
  .replaceAll('__JOLT_LAYOUT__', JSON.stringify(layoutMeta));
writeFileSync(postOutPath, postGen);
console.log(`gen-bindings: wrote ${postOutPath}`);
