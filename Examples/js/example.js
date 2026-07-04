// Shared harness for the (new embind API) JoltPhysics.js examples — a faithful
// port of the original Examples_old/js/example.js, adapted to the new API:
//   * math values are plain arrays ([x,y,z] / [x,y,z,w]); no Jolt.Vec3 objects
//   * transforms are read with the zero-allocation out-param API
//   * bodies are plain numeric ids
//   * bodies render from their real collision geometry via Shape.GetTriangles()
// Same helper names/shape as the original so examples port with minimal changes.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

export const LAYER_NON_MOVING = 0;
export const LAYER_MOVING = 1;
export const NUM_OBJECT_LAYERS = 2;
export const DegreesToRadians = (deg) => deg * (Math.PI / 180.0);

// Live-binding module state (importers see updates) — mirrors the old globals.
export let Jolt, jolt, physicsSystem, bodyInterface;
export let scene, camera, controls, renderer, container;
export let time = 0;
export const dynamicObjects = [];   // three.js meshes; each mesh.userData = { id, com }

let clock, onExampleUpdate = null;
const _m16 = new Float32Array(16);   // reused; zero-alloc transform reads

// A random rotation, as a mathcat quaternion [x,y,z,w].
const _q = new THREE.Quaternion();
const _axis = new THREE.Vector3();
export function getRandomQuat() {
  _axis.set(0.001 + Math.random(), Math.random(), Math.random()).normalize();
  _q.setFromAxisAngle(_axis, 2 * Math.PI * Math.random());
  return [_q.x, _q.y, _q.z, _q.w];
}

function setupCollisionFiltering(settings) {
  const objectFilter = new Jolt.ObjectLayerPairFilterTable(NUM_OBJECT_LAYERS);
  objectFilter.EnableCollision(LAYER_NON_MOVING, LAYER_MOVING);
  objectFilter.EnableCollision(LAYER_MOVING, LAYER_MOVING);

  const NUM_BROAD_PHASE_LAYERS = 2;
  const bpInterface = new Jolt.BroadPhaseLayerInterfaceTable(NUM_OBJECT_LAYERS, NUM_BROAD_PHASE_LAYERS);
  bpInterface.MapObjectToBroadPhaseLayer(LAYER_NON_MOVING, new Jolt.BroadPhaseLayer(0));
  bpInterface.MapObjectToBroadPhaseLayer(LAYER_MOVING, new Jolt.BroadPhaseLayer(1));

  settings.mObjectLayerPairFilter = objectFilter;
  settings.mBroadPhaseLayerInterface = bpInterface;
  const objectVsBpFilter = new Jolt.ObjectVsBroadPhaseLayerFilterTable(bpInterface, NUM_BROAD_PHASE_LAYERS, objectFilter, NUM_OBJECT_LAYERS);
  settings.mObjectVsBroadPhaseLayerFilter = objectVsBpFilter;
  // No retention needed: these filter tables are raw-pointer embind handles (no smart_ptr),
  // which embind never GC-finalizes. The JoltInterface owns them in C++ (its dtor deletes
  // them); the JS wrappers just go stale harmlessly.
}

function initGraphics() {
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setClearColor(0xbfd1e5);
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(window.innerWidth, window.innerHeight);

  camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.2, 2000);
  camera.position.set(0, 15, 30);
  camera.lookAt(0, 0, 0);

  scene = new THREE.Scene();
  const dirLight = new THREE.DirectionalLight(0xffffff, 1);
  dirLight.position.set(10, 10, 5);
  scene.add(dirLight);
  scene.add(new THREE.AmbientLight(0x666666));

  controls = new OrbitControls(camera, renderer.domElement);
  container.appendChild(renderer.domElement);

  window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
  }, false);
}

function initPhysics(customSetup) {
  const settings = new Jolt.JoltSettings();
  (customSetup || setupCollisionFiltering)(settings);
  jolt = new Jolt.JoltInterface(settings);
  physicsSystem = jolt.GetPhysicsSystem();
  bodyInterface = physicsSystem.GetBodyInterface();
}

function updatePhysics(deltaTime) {
  const numSteps = deltaTime > 1.0 / 55.0 ? 2 : 1;   // below 55 Hz -> 2 steps
  jolt.Step(deltaTime, numSteps);
}

// Set/replace the per-frame update callback (time, deltaTime) after initExample.
export function setOnUpdate(fn) { onExampleUpdate = fn; }

// customSetup(settings) — optional; override the default (table-based) collision
// filtering (e.g. the mask-based alternative).
export function initExample(joltModule, updateFunction, customSetup) {
  Jolt = joltModule;
  onExampleUpdate = updateFunction;
  container = document.getElementById('container');
  container.innerHTML = '';
  clock = new THREE.Clock();
  initGraphics();
  initPhysics(customSetup);
  renderExample();
}

function renderExample() {
  requestAnimationFrame(renderExample);
  let deltaTime = clock.getDelta();
  deltaTime = Math.min(deltaTime, 1.0 / 30.0);   // don't go below 30 Hz

  if (onExampleUpdate != null) onExampleUpdate(time, deltaTime);

  // sync every tracked body from its transform (zero-alloc out-param read)
  for (const mesh of dynamicObjects) {
    const u = mesh.userData;
    if (u.softBody) {   // deforming soft body: refresh vertex positions from the sim
      const position = u.geometry.getAttribute('position');
      position.array.set(u.softBody.GetSoftBodyVertices());
      position.needsUpdate = true;
      u.geometry.computeVertexNormals();
    }
    if (u.com) bodyInterface.GetCenterOfMassTransform(_m16, u.id);
    else bodyInterface.GetWorldTransform(_m16, u.id);
    mesh.matrix.fromArray(_m16);
  }

  time += deltaTime;
  updatePhysics(deltaTime);
  controls.update(deltaTime);
  renderer.render(scene, camera);
}

// Build a three.js geometry from a shape's real collision triangles (works for any
// shape: primitives, compounds, decorators). Geometry is center-of-mass-relative.
export function createMeshForShape(shape) {
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute('position', new THREE.BufferAttribute(shape.GetTriangles(), 3));
  geometry.computeVertexNormals();
  return geometry;
}

export function getThreeObjectForBody(id, color) {
  const material = new THREE.MeshPhongMaterial({ color });
  const mesh = new THREE.Mesh(createMeshForShape(bodyInterface.GetShape(id)), material);
  mesh.matrixAutoUpdate = false;
  return mesh;
}

// Add a soft body to the world + scene. Its render mesh uses the static face indices
// from the shared settings; the render loop refreshes vertex positions each frame
// (userData.softBody). Placed at the body's center-of-mass transform (com:true) since
// GetSoftBodyVertices() returns COM-relative positions.
export function addSoftBodyToScene(body, sharedSettings, color = 0xffffff) {
  const id = body.GetID();
  bodyInterface.AddBody(id, Jolt.EActivation.Activate);
  const geometry = new THREE.BufferGeometry();
  const numVertices = body.GetSoftBodyVertices().length / 3;
  geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(numVertices * 3), 3));
  geometry.setIndex(new THREE.BufferAttribute(sharedSettings.GetFaceIndices(), 1));
  const mesh = new THREE.Mesh(geometry, new THREE.MeshPhongMaterial({ color, side: THREE.DoubleSide }));
  mesh.matrixAutoUpdate = false;
  mesh.userData = { id, com: true, softBody: body, geometry };
  scene.add(mesh);
  dynamicObjects.push(mesh);
  return mesh;
}

// Add a created (not-yet-added) body to the physics world + the scene.
export function addToScene(body, color) {
  const id = body.GetID();
  bodyInterface.AddBody(id, Jolt.EActivation.Activate);
  addToThreeScene(id, color);
  return id;
}

export function addToThreeScene(id, color) {
  const mesh = getThreeObjectForBody(id, color);
  mesh.userData = { id, com: true };   // GetTriangles geometry is COM-relative
  scene.add(mesh);
  dynamicObjects.push(mesh);
}

export function removeFromScene(threeObject) {
  const idx = dynamicObjects.indexOf(threeObject);
  if (idx < 0) return;
  const id = threeObject.userData.id;
  bodyInterface.RemoveBody(id);
  bodyInterface.DestroyBody(id);
  scene.remove(threeObject);
  dynamicObjects.splice(idx, 1);
}

export function createFloor(size = 50) {
  const shape = new Jolt.BoxShape([size, 0.5, size]);
  const bcs = new Jolt.BodyCreationSettings(shape, [0, -0.5, 0], [0, 0, 0, 1], Jolt.EMotionType.Static, LAYER_NON_MOVING);
  const body = bodyInterface.CreateBody(bcs);
  addToScene(body, 0xc7c7c7);
  return body;
}

export function createBox(position, rotation, halfExtent, motionType, layer, color = 0xffffff) {
  const shape = new Jolt.BoxShape(halfExtent);
  const bcs = new Jolt.BodyCreationSettings(shape, position, rotation, motionType, layer);
  const body = bodyInterface.CreateBody(bcs);
  addToScene(body, color);
  return body;
}

export function createSphere(position, radius, motionType, layer, color = 0xffffff) {
  const shape = new Jolt.SphereShape(radius);
  const bcs = new Jolt.BodyCreationSettings(shape, position, [0, 0, 0, 1], motionType, layer);
  const body = bodyInterface.CreateBody(bcs);
  addToScene(body, color);
  return body;
}

// A wavy triangle-mesh floor (MeshShapeSettings takes flat vertex + index arrays).
export function createMeshFloor(n, cellSize, maxHeight, posX, posY, posZ) {
  const height = (x, z) => maxHeight * Math.sin(x / 2) * Math.cos(z / 3);
  const verts = [];      // flat [x,y,z, ...] (one per grid corner)
  const indices = [];    // flat triangle indices
  const stride = n + 1;
  const center = n * cellSize / 2;
  for (let x = 0; x <= n; ++x)
    for (let z = 0; z <= n; ++z)
      verts.push(cellSize * x - center, height(x, z), cellSize * z - center);
  for (let x = 0; x < n; ++x)
    for (let z = 0; z < n; ++z) {
      const a = x * stride + z, b = a + 1, c = (x + 1) * stride + z, d = c + 1;
      indices.push(a, b, d, a, d, c);
    }
  const shape = new Jolt.MeshShapeSettings(verts, indices).Create().Get();
  const bcs = new Jolt.BodyCreationSettings(shape, [posX, posY, posZ], [0, 0, 0, 1], Jolt.EMotionType.Static, LAYER_NON_MOVING);
  const body = bodyInterface.CreateBody(bcs);
  addToScene(body, 0xc7c7c7);
  return body;
}

// The vehicle demo race track — convex-hull walls (ported from the original harness).
export function createVehicleTrack() {
  const track = [
		[[[38, 64, -14], [38, 64, -16], [38, -64, -16], [38, -64, -14], [64, -64, -16], [64, -64, -14], [64, 64, -16], [64, 64, -14]], [[-16, 64, -14], [-16, 64, -16], [-16, -64, -16], [-16, -64, -14], [10, -64, -16], [10, -64, -14], [10, 64, -16], [10, 64, -14]], [[10, -48, -14], [10, -48, -16], [10, -64, -16], [10, -64, -14], [38, -64, -16], [38, -64, -14], [38, -48, -16], [38, -48, -14]], [[10, 64, -14], [10, 64, -16], [10, 48, -16], [10, 48, -14], [38, 48, -16], [38, 48, -14], [38, 64, -16], [38, 64, -14]]],
		[[[38, 48, -10], [38, 48, -14], [38, -48, -14], [38, -48, -10], [40, -48, -14], [40, -48, -10], [40, 48, -14], [40, 48, -10]], [[62, 62, -10], [62, 62, -14], [62, -64, -14], [62, -64, -10], [64, -64, -14], [64, -64, -10], [64, 62, -14], [64, 62, -10]], [[8, 48, -10], [8, 48, -14], [8, -48, -14], [8, -48, -10], [10, -48, -14], [10, -48, -10], [10, 48, -14], [10, 48, -10]], [[-16, 62, -10], [-16, 62, -14], [-16, -64, -14], [-16, -64, -10], [-14, -64, -14], [-14, -64, -10], [-14, 62, -14], [-14, 62, -10]], [[-14, -62, -10], [-14, -62, -14], [-14, -64, -14], [-14, -64, -10], [62, -64, -14], [62, -64, -10], [62, -62, -14], [62, -62, -10]], [[8, -48, -10], [8, -48, -14], [8, -50, -14], [8, -50, -10], [40, -50, -14], [40, -50, -10], [40, -48, -14], [40, -48, -10]], [[8, 50, -10], [8, 50, -14], [8, 48, -14], [8, 48, -10], [40, 48, -14], [40, 48, -10], [40, 50, -14], [40, 50, -10]], [[-16, 64, -10], [-16, 64, -14], [-16, 62, -14], [-16, 62, -10], [64, 62, -14], [64, 62, -10], [64, 64, -14], [64, 64, -10]]],
		[[[-4, 22, -14], [-4, -14, -14], [-4, -14, -10], [4, -14, -14], [4, -14, -10], [4, 22, -14]], [[-4, -27, -14], [-4, -48, -14], [-4, -48, -11], [4, -48, -14], [4, -48, -11], [4, -27, -14]], [[-4, 50, -14], [-4, 30, -14], [-4, 30, -12], [4, 30, -14], [4, 30, -12], [4, 50, -14]], [[46, 50, -14], [46, 31, -14], [46, 50, -12], [54, 31, -14], [54, 50, -12], [54, 50, -14]], [[46, 16, -14], [46, -19, -14], [46, 16, -10], [54, -19, -14], [54, 16, -10], [54, 16, -14]], [[46, -28, -14], [46, -48, -14], [46, -28, -11], [54, -48, -14], [54, -28, -11], [54, -28, -14]]]
  ];
  const mapColors = [0x666666, 0x006600, 0x000066];
  const q = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 1, 0), 0.5 * Math.PI);
  const mapRot = [q.x, q.y, q.z, q.w];
  track.forEach((type, tIdx) => {
    type.forEach((block) => {
      const pts = new Jolt.ArrayVec3();
      block.forEach((v) => pts.push_back([-v[1], v[2], v[0]]));
      const shape = new Jolt.ConvexHullShapeSettings(pts).Create().Get();
      pts.delete();
      const bcs = new Jolt.BodyCreationSettings(shape, [0, 10, 0], mapRot, Jolt.EMotionType.Static, LAYER_NON_MOVING);
      const body = bodyInterface.CreateBody(bcs);
      body.SetFriction(1.0);
      addToScene(body, mapColors[tIdx]);
    });
  });
}

// Debug-draw helpers (points are plain [x,y,z] arrays).
export function addLine(from, to, color) {
  const geometry = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(...from), new THREE.Vector3(...to)]);
  scene.add(new THREE.Line(geometry, new THREE.LineBasicMaterial({ color })));
}
export function addMarker(location, size, color) {
  const c = new THREE.Vector3(...location);
  const points = [
    c.clone().add(new THREE.Vector3(-size, 0, 0)), c.clone().add(new THREE.Vector3(size, 0, 0)),
    c.clone().add(new THREE.Vector3(0, -size, 0)), c.clone().add(new THREE.Vector3(0, size, 0)),
    c.clone().add(new THREE.Vector3(0, 0, -size)), c.clone().add(new THREE.Vector3(0, 0, size)),
  ];
  const geometry = new THREE.BufferGeometry().setFromPoints(points);
  scene.add(new THREE.LineSegments(geometry, new THREE.LineBasicMaterial({ color })));
}
