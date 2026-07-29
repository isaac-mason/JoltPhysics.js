// JoltPhysics.js facade — installed into the Emscripten module via --post-js.
//
// This file is a TEMPLATE, not linked directly. scripts/gen-bindings.mjs reads bindings.cpp's
// compiled _outMeta()/_layoutMeta() getters off a probe module and substitutes the two
// placeholder tokens below (the out-param reader list and the packed-buffer stride table) with
// baked literals, emitting post.generated.js — which is what gets linked. So the metadata still
// originates in bindings.cpp, but the shipped runtime never decodes those getters' return values
// (a JSON string over growable wasm memory trips TextDecoder in the browser). The out-param scratch
// pointer (_getOutScratch) is a runtime value and stays a runtime call. (Keep the raw tokens out of
// comments — the build substitutes them by name.)
(function () {
  // Post-js runs on pthread worker threads too, where embind isn't set up. Skip there.
  if (typeof ENVIRONMENT_IS_PTHREAD !== 'undefined' && ENVIRONMENT_IS_PTHREAD) return;

  const outScratch = Module['_getOutScratch']();
  if (!outScratch) { console.error('JoltPhysics.js: _getOutScratch() returned null — out-param readers not installed'); return; }
  const outScratchF32 = outScratch >>> 2;

  function makeOutParamReader(rawIntoMethod, slotSizes, trailingArgCount) {
    const outParams   = slotSizes.map((_, i) => `out${i}`);
    const trailParams = Array.from({length: trailingArgCount}, (_, i) => `arg${i}`);
    const paramList   = [...outParams, ...trailParams].join(', ');

    let byteOffset = 0;
    const scratchSlotPtrs = slotSizes.map(size => {
      const ptr = outScratch + byteOffset;
      byteOffset += size * 4;
      return ptr;
    });

    const intoArgList = [...scratchSlotPtrs, ...trailParams].join(', ');

    const copyLoops = slotSizes.map((size, slotIndex) => {
      const f32Base = outScratchF32 + slotSizes.slice(0, slotIndex).reduce((a, x) => a + x, 0);
      return `for(let i=0;i<${size};i++) out${slotIndex}[i]=h[${f32Base}+i];`;
    }).join('');

    const returnExpr = outParams.length === 1 ? outParams[0] : `[${outParams.join(',')}]`;

    return new Function('rawIntoMethod', 'Module',
      `return function(${paramList}){const h=Module.HEAPF32;rawIntoMethod.call(this,${intoArgList});${copyLoops}return ${returnExpr};}`
    )(rawIntoMethod, Module);
  }

  try {
    // The reader list is baked in at build time (substituted for the token below from _outMeta()).
    // Bracket notation for field access — the baked literal has quoted keys (JSON.stringify), which
    // closure won't rename, so bracket access stays consistent.
    for (const entry of __JOLT_OUT_META__) {
      const cls      = entry['cls'];
      const method   = entry['method'];
      const sizes    = entry['sizes'];
      const trailing = entry['trailing'];
      const proto = Module[cls] && Module[cls].prototype;
      if (!proto) { console.warn(`JoltPhysics.js: _outMeta: unknown class "${cls}" — skipping ${method}`); continue; }
      proto[method] = makeOutParamReader(proto[method + 'Into'], sizes, trailing);
    }
  } catch (e) {
    console.error('JoltPhysics.js: failed to install out-param readers — all GetX(out) calls will malfunction:', e);
  }

  // ---- stride constants (baked from bindings.cpp namespace layout, via _layoutMeta()) ----
  // The field READ order in each reader below must still match the C++ packing order by hand;
  // these strides are the single source of truth so they can't drift. Bracket access: the baked
  // literal has quoted keys closure won't rename.
  const LAYOUT = __JOLT_LAYOUT__;
  const CONTACT_I32_STRIDE = LAYOUT['contactI32']; // body1, body2, subShape1, subShape2, ptStart, ptCount
  const CONTACT_F32_STRIDE = LAYOUT['contactF32']; // normalX, normalY, normalZ, penetrationDepth
  const POINT_F32_STRIDE   = LAYOUT['pointF32'];   // on1(xyz), on2(xyz)
  const REMOVED_I32_STRIDE = LAYOUT['removedI32']; // body1, subShape1, body2, subShape2
  const ACTIVE_BODY_STRIDE = LAYOUT['activeBody']; // id(u32), px,py,pz, rx,ry,rz,rw, lvx,lvy,lvz, avx,avy,avz

  // ---- contact buffer ----
  // Public API names and property names use string-key bracket notation so closure
  // compiler does not rename them.  Embind method calls use dot notation so closure
  // can rename both definition and call site consistently.

  function createContactBuffer(physicsSystem) {
    const impl = new Module['ContactListenerBuffer']();
    physicsSystem['SetContactListener'](impl);
    return {
      '_impl': impl,
      'addedCount': 0, 'persistedCount': 0, 'removedCount': 0,
      _addedI32: null, _addedI32Base: 0,
      _addedF32: null, _addedF32Base: 0,
      _persistedI32: null, _persistedI32Base: 0,
      _persistedF32: null, _persistedF32Base: 0,
      _pointsF32: null, _pointsF32Base: 0,
      _removedI32: null, _removedI32Base: 0,
    };
  }

  function clearContactBuffer(buf) {
    buf['_impl']['Clear']();
    buf['addedCount'] = buf['persistedCount'] = buf['removedCount'] = 0;
  }

  function updateContactBuffer(buf) {
    // Bracket notation: embind method names are runtime string keys closure
    // can't rename (see updateActiveBodyBuffer).
    const impl = buf['_impl'];
    buf['addedCount']     = impl['GetAddedCount']();
    buf['persistedCount'] = impl['GetPersistedCount']();
    buf['removedCount']   = impl['GetRemovedCount']();
    buf._addedI32     = Module['HEAP32'];  buf._addedI32Base     = impl['AddedI32Ptr']()     >>> 2;
    buf._addedF32     = Module['HEAPF32']; buf._addedF32Base     = impl['AddedF32Ptr']()     >>> 2;
    buf._persistedI32 = Module['HEAP32'];  buf._persistedI32Base = impl['PersistedI32Ptr']() >>> 2;
    buf._persistedF32 = Module['HEAPF32']; buf._persistedF32Base = impl['PersistedF32Ptr']() >>> 2;
    buf._pointsF32    = Module['HEAPF32']; buf._pointsF32Base    = impl['PointsF32Ptr']()    >>> 2;
    buf._removedI32   = Module['HEAP32'];  buf._removedI32Base   = impl['RemovedI32Ptr']()   >>> 2;
  }

  function _readContact(i32, i32Base, f32, f32Base, buf, out, i) {
    const iBase = i32Base + i * CONTACT_I32_STRIDE;
    const fBase = f32Base + i * CONTACT_F32_STRIDE;
    out['body1']       = i32[iBase]     >>> 0;
    out['body2']       = i32[iBase + 1] >>> 0;
    out['subShape1']   = i32[iBase + 2] >>> 0;
    out['subShape2']   = i32[iBase + 3] >>> 0;
    out._ptStart       = i32[iBase + 4];
    out['pointCount']  = i32[iBase + 5];
    out['normal'][0]   = f32[fBase];
    out['normal'][1]   = f32[fBase + 1];
    out['normal'][2]   = f32[fBase + 2];
    out['penetration'] = f32[fBase + 3];
    out._buf           = buf;
    return out;
  }

  function getContactBufferAddedAt(buf, out, i) {
    return _readContact(buf._addedI32, buf._addedI32Base, buf._addedF32, buf._addedF32Base, buf, out, i);
  }

  function getContactBufferPersistedAt(buf, out, i) {
    return _readContact(buf._persistedI32, buf._persistedI32Base, buf._persistedF32, buf._persistedF32Base, buf, out, i);
  }

  function getContactBufferRemovedAt(buf, out, i) {
    const iBase = buf._removedI32Base + i * REMOVED_I32_STRIDE;
    const ri = buf._removedI32;
    out['body1']    = ri[iBase]     >>> 0;
    out['subShape1']= ri[iBase + 1] >>> 0;
    out['body2']    = ri[iBase + 2] >>> 0;
    out['subShape2']= ri[iBase + 3] >>> 0;
    return out;
  }

  function getContactBufferPointAt(buf, out, contact, i) {
    const fBase = buf._pointsF32Base + (contact._ptStart + i) * POINT_F32_STRIDE;
    const pf = buf._pointsF32;
    out['on1'][0] = pf[fBase];     out['on1'][1] = pf[fBase + 1]; out['on1'][2] = pf[fBase + 2];
    out['on2'][0] = pf[fBase + 3]; out['on2'][1] = pf[fBase + 4]; out['on2'][2] = pf[fBase + 5];
    return out;
  }

  function destroyContactBuffer(buf) { buf['_impl']['delete'](); }

  // ---- active body buffer ----

  function createActiveBodyBuffer(physicsSystem) {
    return {
      '_impl': new Module['ActiveBodyBuffer'](),
      '_sys':  physicsSystem,
      'bodyCount': 0,
      _f32: null, _f32Base: 0, _u32: null,
    };
  }

  function updateActiveBodyBuffer(buf) {
    // Embind method names are runtime string keys closure can't rename, so call
    // them via bracket notation (dot-access would be renamed and mismatch).
    buf['_impl']['Refresh'](buf['_sys']);
    buf['bodyCount'] = buf['_impl']['GetBodyCount']();
    const base   = buf['_impl']['BodiesF32Ptr']();
    buf._f32Base = base >>> 2;
    buf._f32     = Module['HEAPF32'];
    buf._u32     = Module['HEAPU32'];
  }

  function getActiveBodyBufferStateAt(buf, out, i) {
    const base = buf._f32Base + i * ACTIVE_BODY_STRIDE;
    const f32 = buf._f32, u32 = buf._u32;
    out['id']          = u32[base];
    out['position'][0] = f32[base + 1];  out['position'][1] = f32[base + 2];  out['position'][2] = f32[base + 3];
    out['rotation'][0] = f32[base + 4];  out['rotation'][1] = f32[base + 5];
    out['rotation'][2] = f32[base + 6];  out['rotation'][3] = f32[base + 7];
    out['linVel'][0]   = f32[base + 8];  out['linVel'][1]   = f32[base + 9];  out['linVel'][2]   = f32[base + 10];
    out['angVel'][0]   = f32[base + 11]; out['angVel'][1]   = f32[base + 12]; out['angVel'][2]   = f32[base + 13];
    return out;
  }

  function destroyActiveBodyBuffer(buf) { buf['_impl']['delete'](); }

  // ---- out-param object factories ----

  const createContact         = () => ({ 'body1': 0, 'body2': 0, 'subShape1': 0, 'subShape2': 0,
    'normal': [0, 0, 0], 'penetration': 0, 'pointCount': 0, _ptStart: 0, _buf: null });
  const createContactPoint    = () => ({ 'on1': [0, 0, 0], 'on2': [0, 0, 0] });
  const createRemovedContact  = () => ({ 'body1': 0, 'subShape1': 0, 'body2': 0, 'subShape2': 0 });
  const createActiveBodyState = () => ({ 'id': 0, 'position': [0, 0, 0], 'rotation': [0, 0, 0, 1],
    'linVel': [0, 0, 0], 'angVel': [0, 0, 0] });

  Object.assign(Module, {
    // contact buffer
    'createContactBuffer':         createContactBuffer,
    'clearContactBuffer':          clearContactBuffer,
    'updateContactBuffer':         updateContactBuffer,
    'getContactBufferAddedAt':     getContactBufferAddedAt,
    'getContactBufferPersistedAt': getContactBufferPersistedAt,
    'getContactBufferRemovedAt':   getContactBufferRemovedAt,
    'getContactBufferPointAt':     getContactBufferPointAt,
    'destroyContactBuffer':        destroyContactBuffer,
    // active body buffer
    'createActiveBodyBuffer':      createActiveBodyBuffer,
    'updateActiveBodyBuffer':      updateActiveBodyBuffer,
    'getActiveBodyBufferStateAt':  getActiveBodyBufferStateAt,
    'destroyActiveBodyBuffer':     destroyActiveBodyBuffer,
    // out-param object factories
    'createContact':               createContact,
    'createContactPoint':          createContactPoint,
    'createRemovedContact':        createRemovedContact,
    'createActiveBodyState':       createActiveBodyState,
  });
})();
