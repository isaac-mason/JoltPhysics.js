// Shared harness for the (new embind API) JoltPhysics.js examples.
// Everything is mathcat-style plain arrays; body ids are plain numbers; transforms
// are read with the zero-allocation out-param API straight into three.js matrices.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

export const LAYER_NON_MOVING = 0;
export const LAYER_MOVING = 1;

// Embind handles that the JoltInterface takes OWNERSHIP of (its destructor deletes the
// 3 layer-filter interfaces). Their JS wrappers must stay referenced for the lifetime of
// the world — otherwise the GC finalizes a discarded wrapper, embind calls .delete() on
// it, and the C++ object is freed while the running broadphase still uses it every Step
// -> use-after-free -> hang/crash (intermittent, GC-timing dependent). Keep them alive.
const _worldKeepAlive = [];

// Standard 2-layer collision setup -> a ready-to-use world.
export function createWorld(Jolt) {
  const NUM_OBJ = 2, NUM_BP = 2;
  const objectFilter = new Jolt.ObjectLayerPairFilterTable(NUM_OBJ);
  objectFilter.EnableCollision(LAYER_MOVING, LAYER_NON_MOVING);
  objectFilter.EnableCollision(LAYER_MOVING, LAYER_MOVING);

  const bpInterface = new Jolt.BroadPhaseLayerInterfaceTable(NUM_OBJ, NUM_BP);
  bpInterface.MapObjectToBroadPhaseLayer(LAYER_NON_MOVING, new Jolt.BroadPhaseLayer(0));
  bpInterface.MapObjectToBroadPhaseLayer(LAYER_MOVING, new Jolt.BroadPhaseLayer(1));

  const settings = new Jolt.JoltSettings();
  settings.mObjectLayerPairFilter = objectFilter;
  settings.mBroadPhaseLayerInterface = bpInterface;
  const objectVsBpFilter = new Jolt.ObjectVsBroadPhaseLayerFilterTable(bpInterface, NUM_BP, objectFilter, NUM_OBJ);
  settings.mObjectVsBroadPhaseLayerFilter = objectVsBpFilter;

  const jolt = new Jolt.JoltInterface(settings);
  _worldKeepAlive.push(objectFilter, bpInterface, objectVsBpFilter);   // jolt owns these — never let them be GC-.delete()'d
  const physicsSystem = jolt.GetPhysicsSystem();
  const bodyInterface = physicsSystem.GetBodyInterface();
  return { jolt, physicsSystem, bodyInterface, LAYER_NON_MOVING, LAYER_MOVING };
}

// Minimal three.js scene: camera, orbit controls, lights, shadows, resize, loop.
export function createRenderer(container) {
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x24242e);

  const camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.2, 2000);
  camera.position.set(0, 18, 34);

  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.shadowMap.enabled = true;
  container.innerHTML = '';
  container.appendChild(renderer.domElement);

  const controls = new OrbitControls(camera, renderer.domElement);
  controls.target.set(0, 4, 0);
  controls.update();

  scene.add(new THREE.HemisphereLight(0xffffff, 0x334455, 0.6));
  const dir = new THREE.DirectionalLight(0xffffff, 2.0);
  dir.position.set(12, 24, 8);
  dir.castShadow = true;
  dir.shadow.mapSize.set(2048, 2048);
  const s = dir.shadow.camera;
  s.left = s.bottom = -40; s.right = s.top = 40; s.near = 1; s.far = 120;
  scene.add(dir);

  window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
  });

  let onFrame = null;
  const clock = new THREE.Clock();
  function loop() {
    requestAnimationFrame(loop);
    const dt = Math.min(clock.getDelta(), 1 / 30);
    if (onFrame) onFrame(dt);
    controls.update();
    renderer.render(scene, camera);
  }
  return { scene, camera, renderer, controls, setOnFrame: (cb) => { onFrame = cb; }, start: loop };
}

// Track a body + its mesh; sync the mesh from the body's world transform each frame
// using the zero-allocation out-param API (one wasm call per body, straight to the
// three.js matrix — the showcase of the new API). Pass { com: true } for meshes whose
// geometry is center-of-mass-relative (e.g. built by meshFromShape).
export function createBodySync(bodyInterface, scene) {
  const objects = [];
  const m16 = new Float32Array(16);   // reused; zero allocation per frame

  return {
    add(id, mesh, { com = false } = {}) {
      mesh.matrixAutoUpdate = false;
      scene.add(mesh);
      objects.push({ id, mesh, com });
    },
    update() {
      for (const o of objects) {
        if (o.com) bodyInterface.GetCenterOfMassTransform(m16, o.id);
        else bodyInterface.GetWorldTransform(m16, o.id);   // out-param -> the matrix array
        o.mesh.matrix.fromArray(m16);
      }
    },
    get count() { return objects.length; },
  };
}

// Build a three.js mesh straight from a Jolt shape's triangles (Shape.GetTriangles):
// works for ANY shape — meshes, convex hulls, heightfields, compounds — not just the
// ones that map to a THREE primitive. Geometry is center-of-mass-relative, so sync the
// resulting mesh with { com: true }.
export function meshFromShape(shape, material) {
  const verts = shape.GetTriangles();   // flat Float32Array, 9 floats per triangle
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(verts, 3));
  geometry.computeVertexNormals();
  const mesh = new THREE.Mesh(geometry, material);
  mesh.castShadow = mesh.receiveShadow = true;
  return mesh;
}
