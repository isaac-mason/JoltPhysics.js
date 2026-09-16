// TypeScript definitions for JoltPhysics.js. Auto-generated (embind --emit-tsd +
// scripts/gen-bindings.mjs). Do not edit by hand.
interface WasmModule {
}

type EmbindString = ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array|string;
export interface ClassHandle {
  isAliasOf(other: ClassHandle): boolean;
  delete(): void;
  deleteLater(): this;
  isDeleted(): boolean;
  // @ts-ignore - If targeting lower than ESNext, this symbol might not exist.
  [Symbol.dispose](): void;
  clone(): this;
}
export interface EBodyTypeValue<T extends number> {
  value: T;
}
export type EBodyType = EBodyTypeValue<0>|EBodyTypeValue<1>;

export interface EMotionTypeValue<T extends number> {
  value: T;
}
export type EMotionType = EMotionTypeValue<0>|EMotionTypeValue<1>|EMotionTypeValue<2>;

export interface EMotionQualityValue<T extends number> {
  value: T;
}
export type EMotionQuality = EMotionQualityValue<0>|EMotionQualityValue<1>;

export interface EActivationValue<T extends number> {
  value: T;
}
export type EActivation = EActivationValue<0>|EActivationValue<1>;

export interface EShapeTypeValue<T extends number> {
  value: T;
}
export type EShapeType = EShapeTypeValue<0>|EShapeTypeValue<1>|EShapeTypeValue<2>|EShapeTypeValue<3>|EShapeTypeValue<4>|EShapeTypeValue<10>|EShapeTypeValue<11>;

export interface EShapeSubTypeValue<T extends number> {
  value: T;
}
export type EShapeSubType = EShapeSubTypeValue<0>|EShapeSubTypeValue<1>|EShapeSubTypeValue<3>|EShapeSubTypeValue<4>|EShapeSubTypeValue<5>|EShapeSubTypeValue<32>|EShapeSubTypeValue<6>|EShapeSubTypeValue<7>|EShapeSubTypeValue<8>|EShapeSubTypeValue<9>|EShapeSubTypeValue<10>|EShapeSubTypeValue<11>|EShapeSubTypeValue<12>|EShapeSubTypeValue<13>|EShapeSubTypeValue<31>|EShapeSubTypeValue<33>;

export interface EConstraintSpaceValue<T extends number> {
  value: T;
}
export type EConstraintSpace = EConstraintSpaceValue<0>|EConstraintSpaceValue<1>;

export interface ESpringModeValue<T extends number> {
  value: T;
}
export type ESpringMode = ESpringModeValue<0>|ESpringModeValue<1>;

export interface EOverrideMassPropertiesValue<T extends number> {
  value: T;
}
export type EOverrideMassProperties = EOverrideMassPropertiesValue<0>|EOverrideMassPropertiesValue<1>|EOverrideMassPropertiesValue<2>;

export interface EAllowedDOFsValue<T extends number> {
  value: T;
}
export type EAllowedDOFs = EAllowedDOFsValue<1>|EAllowedDOFsValue<2>|EAllowedDOFsValue<4>|EAllowedDOFsValue<8>|EAllowedDOFsValue<16>|EAllowedDOFsValue<32>|EAllowedDOFsValue<35>|EAllowedDOFsValue<63>;

export interface EStateRecorderStateValue<T extends number> {
  value: T;
}
export type EStateRecorderState = EStateRecorderStateValue<0>|EStateRecorderStateValue<1>|EStateRecorderStateValue<2>|EStateRecorderStateValue<4>|EStateRecorderStateValue<8>|EStateRecorderStateValue<15>;

export interface EBackFaceModeValue<T extends number> {
  value: T;
}
export type EBackFaceMode = EBackFaceModeValue<0>|EBackFaceModeValue<1>;

export interface EGroundStateValue<T extends number> {
  value: T;
}
export type EGroundState = EGroundStateValue<0>|EGroundStateValue<1>|EGroundStateValue<2>|EGroundStateValue<3>;

export interface ValidateResultValue<T extends number> {
  value: T;
}
export type ValidateResult = ValidateResultValue<0>|ValidateResultValue<1>|ValidateResultValue<2>|ValidateResultValue<3>;

export interface SoftBodyValidateResultValue<T extends number> {
  value: T;
}
export type SoftBodyValidateResult = SoftBodyValidateResultValue<0>|SoftBodyValidateResultValue<1>;

export interface EActiveEdgeModeValue<T extends number> {
  value: T;
}
export type EActiveEdgeMode = EActiveEdgeModeValue<0>|EActiveEdgeModeValue<1>;

export interface ECollectFacesModeValue<T extends number> {
  value: T;
}
export type ECollectFacesMode = ECollectFacesModeValue<0>|ECollectFacesModeValue<1>;

export interface SixDOFConstraintSettings_EAxisValue<T extends number> {
  value: T;
}
export type SixDOFConstraintSettings_EAxis = SixDOFConstraintSettings_EAxisValue<0>|SixDOFConstraintSettings_EAxisValue<1>|SixDOFConstraintSettings_EAxisValue<2>|SixDOFConstraintSettings_EAxisValue<3>|SixDOFConstraintSettings_EAxisValue<4>|SixDOFConstraintSettings_EAxisValue<5>;

export interface EConstraintTypeValue<T extends number> {
  value: T;
}
export type EConstraintType = EConstraintTypeValue<0>|EConstraintTypeValue<1>;

export interface EConstraintSubTypeValue<T extends number> {
  value: T;
}
export type EConstraintSubType = EConstraintSubTypeValue<0>|EConstraintSubTypeValue<1>|EConstraintSubTypeValue<2>|EConstraintSubTypeValue<3>|EConstraintSubTypeValue<4>|EConstraintSubTypeValue<5>|EConstraintSubTypeValue<6>|EConstraintSubTypeValue<7>|EConstraintSubTypeValue<8>|EConstraintSubTypeValue<9>|EConstraintSubTypeValue<10>|EConstraintSubTypeValue<11>|EConstraintSubTypeValue<12>;

export interface EMotorStateValue<T extends number> {
  value: T;
}
export type EMotorState = EMotorStateValue<0>|EMotorStateValue<1>|EMotorStateValue<2>;

export interface ETransmissionModeValue<T extends number> {
  value: T;
}
export type ETransmissionMode = ETransmissionModeValue<0>|ETransmissionModeValue<1>;

export interface ESwingTypeValue<T extends number> {
  value: T;
}
export type ESwingType = ESwingTypeValue<0>|ESwingTypeValue<1>;

export interface EPathRotationConstraintTypeValue<T extends number> {
  value: T;
}
export type EPathRotationConstraintType = EPathRotationConstraintTypeValue<0>|EPathRotationConstraintTypeValue<1>|EPathRotationConstraintTypeValue<2>|EPathRotationConstraintTypeValue<3>|EPathRotationConstraintTypeValue<4>|EPathRotationConstraintTypeValue<5>;

export interface EBendTypeValue<T extends number> {
  value: T;
}
export type EBendType = EBendTypeValue<0>|EBendTypeValue<1>|EBendTypeValue<2>;

export interface ELRATypeValue<T extends number> {
  value: T;
}
export type ELRAType = ELRATypeValue<0>|ELRATypeValue<1>|ELRATypeValue<2>;

export interface MeshShapeSettings_EBuildQualityValue<T extends number> {
  value: T;
}
export type MeshShapeSettings_EBuildQuality = MeshShapeSettings_EBuildQualityValue<0>|MeshShapeSettings_EBuildQualityValue<1>;

export interface Shape extends ClassHandle {
  GetType(): EShapeType;
  GetSubType(): EShapeSubType;
  GetMassProperties(): MassProperties;
  MustBeStatic(): boolean;
  GetRefCount(): number;
  GetUserData(): number;
  SetUserData(userData: number): void;
  GetSubShapeIDBitsRecursive(): number;
  GetLeafShape(subShapeID: number): Shape | null;
  GetMaterial(subShapeID: number): PhysicsMaterial | null;
  GetSubShapeUserData(subShapeID: number): number;
  GetCenterOfMass(out: Vec3): Vec3;
  GetLocalBounds(out: AABox): AABox;
  IsValidScale(scale: Vec3): boolean;
  MakeScaleValid(scale: Vec3): Vec3;
  ScaleShape(scale: Vec3): ShapeResult;
  GetSurfaceNormal(out: Vec3, subShapeID: number, localSurfacePosition: Vec3): Vec3;
  GetSubShapeTransformedShape(subShapeID: number, positionCOM: Vec3, rotation: Quat, scale: Vec3): TransformedShape;
  GetWorldSpaceBounds(out: AABox, comTransform: Mat44, scale: Vec3): AABox;
  GetVolume(): number;
  GetInnerRadius(): number;
  GetStats(): { sizeBytes: number; numTriangles: number };
  GetTriangles(): Float32Array;
}

export interface ConvexShape extends Shape {
  SetMaterial(material: PhysicsMaterial | null): void;
  GetDensity(): number;
  SetDensity(density: number): void;
}

export interface BoxShape extends ConvexShape {
  GetHalfExtent(out: Vec3): Vec3;
}

export interface SphereShape extends ConvexShape {
  GetRadius(): number;
}

export interface ShapeResult extends ClassHandle {
  Get(): Shape | null;
  IsValid(): boolean;
  HasError(): boolean;
  GetError(): string;
}

export interface ShapeSettings extends ClassHandle {
  Create(): ShapeResult;
}

export interface ConvexShapeSettings extends ShapeSettings {
  mDensity: number;
  SetMaterial(material: PhysicsMaterial | null): void;
}

export interface BoxShapeSettings extends ConvexShapeSettings {
  mHalfExtent: Vec3;
  mConvexRadius: number;
}

export interface SphereShapeSettings extends ConvexShapeSettings {
  mRadius: number;
}

export interface CapsuleShape extends ConvexShape {
  GetRadius(): number;
  GetHalfHeightOfCylinder(): number;
}

export interface CapsuleShapeSettings extends ConvexShapeSettings {
  mRadius: number;
  mHalfHeightOfCylinder: number;
}

export interface TaperedCapsuleShape extends ConvexShape {
  GetTopRadius(): number;
  GetBottomRadius(): number;
  GetHalfHeight(): number;
}

export interface TaperedCapsuleShapeSettings extends ConvexShapeSettings {
  mHalfHeightOfTaperedCylinder: number;
  mTopRadius: number;
  mBottomRadius: number;
}

export interface CylinderShape extends ConvexShape {
  GetHalfHeight(): number;
  GetRadius(): number;
}

export interface CylinderShapeSettings extends ConvexShapeSettings {
  mHalfHeight: number;
  mRadius: number;
  mConvexRadius: number;
}

export interface TaperedCylinderShape extends ConvexShape {
  GetTopRadius(): number;
  GetBottomRadius(): number;
  GetConvexRadius(): number;
  GetHalfHeight(): number;
}

export interface TaperedCylinderShapeSettings extends ConvexShapeSettings {
  mHalfHeight: number;
  mTopRadius: number;
  mBottomRadius: number;
  mConvexRadius: number;
}

export interface ConvexHullShape extends ConvexShape {
  GetNumPoints(): number;
  GetPoint(out: Vec3, index: number): Vec3;
}

export interface ConvexHullShapeSettings extends ConvexShapeSettings {
  mMaxConvexRadius: number;
  mHullTolerance: number;
}

export interface CompoundShapeSubShape extends ClassHandle {
  mUserData: number;
  GetShape(): Shape | null;
  GetPositionCOM(out: Vec3): Vec3;
  GetRotation(out: Quat): Quat;
}

export interface CompoundShape extends Shape {
  GetNumSubShapes(): number;
  GetSubShape(index: number): CompoundShapeSubShape | null;
  GetCompoundUserData(index: number): number;
  SetCompoundUserData(index: number, userData: number): void;
  GetCenterOfMass(out: Vec3): Vec3;
}

export interface CompoundShapeSettings extends ShapeSettings {
  AddShape(position: Vec3, rotation: Quat, shape: Shape | null): void;
  AddShapeWithUserData(position: Vec3, rotation: Quat, shape: Shape | null, userData: number): void;
}

export interface StaticCompoundShape extends CompoundShape {
}

export interface StaticCompoundShapeSettings extends CompoundShapeSettings {
}

export interface MutableCompoundShape extends CompoundShape {
  AdjustCenterOfMass(): void;
  RemoveShape(index: number): void;
  AddShape(position: Vec3, rotation: Quat, shape: Shape | null): number;
  AddShapeWithUserData(position: Vec3, rotation: Quat, shape: Shape | null, userData: number): number;
  ModifyShape(index: number, position: Vec3, rotation: Quat): void;
  ModifyShapeShape(index: number, position: Vec3, rotation: Quat, shape: Shape | null): void;
  ModifyShapes(startIndex: number, numShapes: number, positions: any, rotations: any): void;
}

export interface MutableCompoundShapeSettings extends CompoundShapeSettings {
}

export interface DecoratedShape extends Shape {
  GetInnerShape(): Shape | null;
}

export interface DecoratedShapeSettings extends ShapeSettings {
}

export interface RotatedTranslatedShape extends DecoratedShape {
  GetPosition(out: Vec3): Vec3;
  GetRotation(out: Quat): Quat;
}

export interface RotatedTranslatedShapeSettings extends DecoratedShapeSettings {
  mPosition: Vec3;
  mRotation: Quat;
}

export interface ScaledShape extends DecoratedShape {
  GetScale(out: Vec3): Vec3;
}

export interface ScaledShapeSettings extends DecoratedShapeSettings {
  mScale: Vec3;
}

export interface OffsetCenterOfMassShape extends DecoratedShape {
  GetOffset(out: Vec3): Vec3;
}

export interface OffsetCenterOfMassShapeSettings extends DecoratedShapeSettings {
  mOffset: Vec3;
}

export interface EmptyShape extends Shape {
}

export interface EmptyShapeSettings extends ShapeSettings {
  mCenterOfMass: Vec3;
}

export interface Plane extends ClassHandle {
  GetNormal(out: Vec3): Vec3;
  SetNormal(normal: Vec3): void;
  ProjectPointOnPlane(out: Vec3, point: Vec3): Vec3;
  Scaled(scale: Vec3): Plane;
  GetTransformed(transform: Mat44): Plane;
  GetConstant(): number;
  SetConstant(constant: number): void;
  SignedDistance(point: Vec3): number;
  Offset(distance: number): Plane;
}

export interface MeshShape extends Shape {
}

export interface MeshShapeSettings extends ShapeSettings {
  mMaxTrianglesPerLeaf: number;
}

export interface HeightFieldShape extends Shape {
  GetSampleCount(): number;
  IsNoCollision(x: number, y: number): boolean;
  GetPosition(out: Vec3, x: number, y: number): Vec3;
  GetHeights(x: number, y: number, sizeX: number, sizeY: number): Float32Array;
  SetHeights(x: number, y: number, sizeX: number, sizeY: number, heights: any, jolt: JoltInterface): void;
}

export interface HeightFieldShapeSettings extends ShapeSettings {
  mBlockSize: number;
  mBitsPerSample: number;
  mOffset: Vec3;
  mScale: Vec3;
}

export interface PlaneShape extends Shape {
  GetPlane(): Plane;
  GetHalfExtent(): number;
}

export interface PlaneShapeSettings extends ShapeSettings {
  mHalfExtent: number;
}

export interface TransformedShape extends ClassHandle {
  GetShape(): Shape | null;
  CastRayCollide(ray: RRayCast, settings: RayCastSettings, collector: CastRayCollector, shapeFilter: ShapeFilter): void;
  SetShape(shape: Shape | null): void;
  GetMaterial(subShapeID: number): PhysicsMaterial | null;
  GetSubShapeUserData(subShapeID: number): number;
  GetBodyID(): number;
  GetShapeScale(out: Vec3): Vec3;
  GetCenterOfMassTransform(out: Mat44): Mat44;
  GetInverseCenterOfMassTransform(out: Mat44): Mat44;
  GetWorldTransform(out: Mat44): Mat44;
  GetWorldSpaceBounds(out: AABox): AABox;
  GetShapePositionCOM(out: Vec3): Vec3;
  GetShapeRotation(out: Quat): Quat;
  CollidePoint(point: Vec3, collector: CollidePointCollector, shapeFilter: ShapeFilter): void;
  CastShape(shapeCast: RShapeCast, settings: ShapeCastSettings, baseOffset: Vec3, collector: CastShapeCollector, shapeFilter: ShapeFilter): void;
  SetShapeScale(scale: Vec3): void;
  GetWorldSpaceSurfaceNormal(out: Vec3, subShapeID: number, position: Vec3): Vec3;
  SetShapePositionCOM(pos: Vec3): void;
  SetWorldTransform(position: Vec3, rotation: Quat, scale: Vec3): void;
  SetShapeRotation(rot: Quat): void;
  CollideShape(shape: Shape | null, shapeScale: Vec3, comTransform: Mat44, settings: CollideShapeSettings, baseOffset: Vec3, collector: CollideShapeCollector, shapeFilter: ShapeFilter): void;
  SetWorldTransformMat(transform: Mat44): void;
  CastRay(origin: Vec3, direction: Vec3): { fraction: number; point: Vec3 } | null;
  GetSupportingFace(subShapeID: number, direction: Vec3, baseOffset: Vec3): Float32Array;
}

export interface SoftBodySharedSettings extends ClassHandle {
  Clone(): SoftBodySharedSettings | null;
  CalculateEdgeLengths(): void;
  CalculateVolumeConstraintVolumes(): void;
  Optimize(): void;
  AddFaceStruct(face: SoftBodySharedSettingsFace): void;
  AddVertexStruct(vertex: SoftBodySharedSettingsVertex): void;
  AddEdgeConstraintStruct(edge: SoftBodySharedSettingsEdge): void;
  AddDihedralBendConstraint(bend: SoftBodySharedSettingsDihedralBend): void;
  AddVolumeConstraintStruct(volume: SoftBodySharedSettingsVolume): void;
  AddRod(rod: SoftBodySharedSettingsRodStretchShear): void;
  AddRodBendTwist(rod: SoftBodySharedSettingsRodBendTwist): void;
  AddLRAConstraint(lra: SoftBodySharedSettingsLRA): void;
  AddSkinnedConstraint(skinned: SoftBodySharedSettingsSkinned): void;
  AddInvBindMatrix(invBind: SoftBodySharedSettingsInvBind): void;
  CalculateBendConstraintConstants(): void;
  CalculateSkinnedConstraintNormals(): void;
  CalculateRodProperties(): void;
  AddFace(vertex0: number, vertex1: number, vertex2: number): void;
  GetNumVertices(): number;
  GetVertexStruct(index: number): SoftBodySharedSettingsVertex | null;
  GetNumFaces(): number;
  GetFace(index: number): SoftBodySharedSettingsFace | null;
  GetNumEdgeConstraints(): number;
  GetEdgeConstraint(index: number): SoftBodySharedSettingsEdge | null;
  GetNumVolumeConstraints(): number;
  GetVolumeConstraint(index: number): SoftBodySharedSettingsVolume | null;
  GetNumDihedralBendConstraints(): number;
  GetDihedralBendConstraint(index: number): SoftBodySharedSettingsDihedralBend | null;
  GetNumSkinnedConstraints(): number;
  GetSkinnedConstraint(index: number): SoftBodySharedSettingsSkinned | null;
  GetNumInvBindMatrices(): number;
  GetInvBindMatrix(index: number): SoftBodySharedSettingsInvBind | null;
  GetNumLRAConstraints(): number;
  GetLRAConstraint(index: number): SoftBodySharedSettingsLRA | null;
  GetNumRodStretchShearConstraints(): number;
  GetRodStretchShearConstraint(index: number): SoftBodySharedSettingsRodStretchShear | null;
  GetNumRodBendTwistConstraints(): number;
  GetRodBendTwistConstraint(index: number): SoftBodySharedSettingsRodBendTwist | null;
  GetNumMaterials(): number;
  GetMaterial(index: number): PhysicsMaterial | null;
  SetMaterial(index: number, material: PhysicsMaterial | null): void;
  AddMaterial(material: PhysicsMaterial | null): number;
  SetAllVertexVelocities(velocity: Vec3): void;
  AddVertex(position: Vec3, invMass: number): void;
  AddEdgeConstraint(vertex0: number, vertex1: number, compliance: number): void;
  AddVolumeConstraint(vertex0: number, vertex1: number, vertex2: number, vertex3: number, compliance: number): void;
  CreateConstraints(compliance: number, shearCompliance: number, bendCompliance: number, bendType: EBendType, lraType: ELRAType, lraMaxDistanceMultiplier: number): void;
  CalculateLRALengths(maxDistanceMultiplier: number): void;
  GetFaceIndices(): Uint32Array;
  CreateConstraintsFromAttributes(attributesArray: any, bendType: EBendType, angleTolerance: number): void;
}

export interface SoftBodyCreationSettings extends ClassHandle {
  mUpdatePosition: boolean;
  mMakeRotationIdentity: boolean;
  mAllowSleeping: boolean;
  mFacesDoubleSided: boolean;
  mObjectLayer: number;
  mNumIterations: number;
  mUserData: bigint;
  mPosition: Vec3;
  mRotation: Quat;
  mPressure: number;
  mLinearDamping: number;
  mVertexRadius: number;
  mFriction: number;
  mRestitution: number;
  mGravityFactor: number;
  mMaxLinearVelocity: number;
  GetSettings(): SoftBodySharedSettings | null;
  GetCollisionGroup(): CollisionGroup | null;
  SetSettings(settings: SoftBodySharedSettings | null): void;
}

export interface SoftBodyVertex extends ClassHandle {
  HasContact(): boolean;
  GetCollidingShapeIndex(): number;
  GetPosition(out: Vec3): Vec3;
  GetPreviousPosition(out: Vec3): Vec3;
  GetVelocity(out: Vec3): Vec3;
  SetPosition(v: Vec3): void;
  SetVelocity(v: Vec3): void;
  GetInvMass(): number;
  SetInvMass(invMass: number): void;
  GetLargestPenetration(): number;
}

export interface SoftBodyContactSettings extends ClassHandle {
  mIsSensor: boolean;
  mInvMassScale1: number;
  mInvMassScale2: number;
  mInvInertiaScale2: number;
}

export interface SoftBodyManifold extends ClassHandle {
  HasContact(vertex: SoftBodyVertex): boolean;
  GetContactBodyID(vertex: SoftBodyVertex): number;
  GetNumSensorContacts(): number;
  GetSensorContactBodyID(index: number): number;
  GetNumVertices(): number;
  GetVertex(index: number): SoftBodyVertex | null;
  GetLocalContactPoint(out: Vec3, vertex: SoftBodyVertex): Vec3;
  GetContactNormal(out: Vec3, vertex: SoftBodyVertex): Vec3;
}

export interface SoftBodyShape extends Shape {
  GetSubShapeIDBits(): number;
  GetFaceIndex(subShapeID: number): number;
  GetLocalBounds(out: AABox): AABox;
  GetVolume(): number;
}

export interface SoftBodySharedSettingsVertex extends ClassHandle {
  mInvMass: number;
  GetPosition(out: Vec3): Vec3;
  GetVelocity(out: Vec3): Vec3;
  SetPosition(v: Vec3): void;
  SetVelocity(v: Vec3): void;
}

export interface SoftBodySharedSettingsFace extends ClassHandle {
  mMaterialIndex: number;
  GetVertex(index: number): number;
  SetVertex(index: number, value: number): void;
}

export interface SoftBodySharedSettingsEdge extends ClassHandle {
  mRestLength: number;
  mCompliance: number;
  GetVertex(index: number): number;
  SetVertex(index: number, value: number): void;
}

export interface SoftBodySharedSettingsDihedralBend extends ClassHandle {
  mCompliance: number;
  mInitialAngle: number;
  GetVertex(index: number): number;
  SetVertex(index: number, value: number): void;
}

export interface SoftBodySharedSettingsVolume extends ClassHandle {
  mSixRestVolume: number;
  mCompliance: number;
  GetVertex(index: number): number;
  SetVertex(index: number, value: number): void;
}

export interface SoftBodySharedSettingsInvBind extends ClassHandle {
  mJointIndex: number;
  GetInvBind(out: Mat44): Mat44;
  SetInvBind(m: Mat44): void;
}

export interface SoftBodySharedSettingsSkinWeight extends ClassHandle {
  mInvBindIndex: number;
  mWeight: number;
}

export interface SoftBodySharedSettingsSkinned extends ClassHandle {
  mVertex: number;
  mMaxDistance: number;
  mBackStopDistance: number;
  mBackStopRadius: number;
  NormalizeWeights(): void;
  GetWeight(index: number): SoftBodySharedSettingsSkinWeight | null;
  SetWeight(index: number, invBindIndex: number, weight: number): void;
}

export interface SoftBodySharedSettingsLRA extends ClassHandle {
  mMaxDistance: number;
  GetVertex(index: number): number;
  SetVertex(index: number, value: number): void;
}

export interface SoftBodySharedSettingsRodStretchShear extends ClassHandle {
  mLength: number;
  mInvMass: number;
  mCompliance: number;
  GetVertex(index: number): number;
  SetVertex(index: number, value: number): void;
  GetBishop(out: Quat): Quat;
  SetBishop(q: Quat): void;
}

export interface SoftBodySharedSettingsRodBendTwist extends ClassHandle {
  mCompliance: number;
  GetRod(index: number): number;
  SetRod(index: number, value: number): void;
  GetOmega0(out: Quat): Quat;
  SetOmega0(q: Quat): void;
}

export interface SoftBodySharedSettingsVertexAttributes extends ClassHandle {
  mLRAType: ELRAType;
  mCompliance: number;
  mShearCompliance: number;
  mBendCompliance: number;
  mLRAMaxDistanceMultiplier: number;
}

export interface SoftBodyContactListener extends ClassHandle {
}

export interface SoftBodyContactListenerWrapper extends SoftBodyContactListener {
  notifyOnDestruction(): void;
}

export interface BodyCreationSettings extends ClassHandle {
  mMotionType: EMotionType;
  mAllowedDOFs: EAllowedDOFs;
  mMotionQuality: EMotionQuality;
  mOverrideMassProperties: EOverrideMassProperties;
  mAllowDynamicOrKinematic: boolean;
  mIsSensor: boolean;
  mCollideKinematicVsNonDynamic: boolean;
  mUseManifoldReduction: boolean;
  mApplyGyroscopicForce: boolean;
  mEnhancedInternalEdgeRemoval: boolean;
  mAllowSleeping: boolean;
  mObjectLayer: number;
  mNumVelocityStepsOverride: number;
  mNumPositionStepsOverride: number;
  mUserData: bigint;
  mPosition: Vec3;
  mLinearVelocity: Vec3;
  mAngularVelocity: Vec3;
  mRotation: Quat;
  mFriction: number;
  mRestitution: number;
  mLinearDamping: number;
  mAngularDamping: number;
  mMaxLinearVelocity: number;
  mMaxAngularVelocity: number;
  mGravityFactor: number;
  mInertiaMultiplier: number;
  GetShapeSettings(): ShapeSettings | null;
  GetShape(): Shape | null;
  GetCollisionGroup(): CollisionGroup | null;
  GetMassProperties(): MassProperties;
  GetMassPropertiesOverride(): MassProperties | null;
  SetShapeSettings(shape: ShapeSettings | null): void;
  SetShape(shape: Shape | null): void;
  HasMassProperties(): boolean;
}

export interface BodyInterface extends ClassHandle {
  CreateBody(settings: BodyCreationSettings): Body | null;
  CreateSoftBody(settings: SoftBodyCreationSettings): Body | null;
  CreateBodyWithoutID(settings: BodyCreationSettings): Body | null;
  CreateSoftBodyWithoutID(settings: SoftBodyCreationSettings): Body | null;
  DestroyBodyWithoutID(body: Body | null): void;
  ActivateConstraint(constraint: TwoBodyConstraint | null): void;
  AssignBodyID(body: Body | null): boolean;
  CreateAndAddBody(settings: BodyCreationSettings, activationMode: EActivation): number;
  AddBody(bodyID: number, activationMode: EActivation): void;
  RemoveBody(bodyID: number): void;
  DestroyBody(bodyID: number): void;
  ActivateBody(bodyID: number): void;
  DeactivateBody(bodyID: number): void;
  IsActive(bodyID: number): boolean;
  SetMotionType(bodyID: number, motionType: EMotionType, activationMode: EActivation): void;
  IsAdded(bodyID: number): boolean;
  GetBodyType(bodyID: number): EBodyType;
  GetMotionType(bodyID: number): EMotionType;
  GetMotionQuality(bodyID: number): EMotionQuality;
  GetObjectLayer(bodyID: number): number;
  SetMotionQuality(bodyID: number, motionQuality: EMotionQuality): void;
  SetObjectLayer(bodyID: number, layer: number): void;
  GetShape(bodyID: number): Shape | null;
  SetShape(bodyID: number, shape: Shape | null, updateMassProperties: boolean, activationMode: EActivation): void;
  GetTransformedShape(bodyID: number): TransformedShape;
  CreateSoftBodyWithID(bodyID: number, settings: SoftBodyCreationSettings): Body | null;
  CreateBodyWithID(bodyID: number, settings: BodyCreationSettings): Body | null;
  CreateAndAddSoftBody(settings: SoftBodyCreationSettings, activationMode: EActivation): number;
  AssignBodyIDWithID(body: Body | null, bodyID: number): boolean;
  UnassignBodyID(bodyID: number): Body | null;
  CreateConstraint(settings: TwoBodyConstraintSettings | null, bodyID1: number, bodyID2: number): TwoBodyConstraint | null;
  ResetSleepTimer(bodyID: number): void;
  SetUseManifoldReduction(bodyID: number, useReduction: boolean): void;
  GetUseManifoldReduction(bodyID: number): boolean;
  SetIsSensor(bodyID: number, isSensor: boolean): void;
  IsSensor(bodyID: number): boolean;
  SetCollisionGroup(bodyID: number, collisionGroup: CollisionGroup): void;
  GetCollisionGroup(bodyID: number): CollisionGroup | null;
  GetMaterial(bodyID: number, subShapeID: number): PhysicsMaterial | null;
  InvalidateContactCache(bodyID: number): void;
  GetPosition(out: Vec3, bodyID: number): Vec3;
  GetCenterOfMassPosition(out: Vec3, bodyID: number): Vec3;
  GetRotation(out: Quat, bodyID: number): Quat;
  GetLinearVelocity(out: Vec3, bodyID: number): Vec3;
  GetAngularVelocity(out: Vec3, bodyID: number): Vec3;
  GetWorldTransform(out: Mat44, bodyID: number): Mat44;
  GetCenterOfMassTransform(out: Mat44, bodyID: number): Mat44;
  DestroyBodies(bodyIDsPtr: number, number: number): void;
  ActivateBodies(bodyIDsPtr: number, number: number): void;
  DeactivateBodies(bodyIDsPtr: number, number: number): void;
  RemoveBodies(bodyIDsPtr: number, number: number): void;
  AddBodiesPrepare(bodyIDsPtr: number, number: number): number;
  AddBodiesFinalize(bodyIDsPtr: number, number: number, addState: number, activationMode: EActivation): void;
  AddBodiesAbort(bodyIDsPtr: number, number: number, addState: number): void;
  GetPositionAndRotationInto(outPos: number, outRot: number, bodyID: number): void;
  GetLinearAndAngularVelocityInto(outLin: number, outAng: number, bodyID: number): void;
  GetInverseInertia(out: Mat44, bodyID: number): Mat44;
  GetUserData(bodyID: number): bigint;
  SetUserData(bodyID: number, userData: bigint): void;
  SetPosition(bodyID: number, position: Vec3, activationMode: EActivation): void;
  SetLinearVelocity(bodyID: number, linearVelocity: Vec3): void;
  SetAngularVelocity(bodyID: number, angularVelocity: Vec3): void;
  AddForce(bodyID: number, force: Vec3): void;
  AddForce(bodyID: number, force: Vec3, point: Vec3): void;
  AddTorque(bodyID: number, torque: Vec3): void;
  AddForceAndTorque(bodyID: number, force: Vec3, torque: Vec3): void;
  AddImpulse(bodyID: number, impulse: Vec3): void;
  AddImpulse(bodyID: number, impulse: Vec3, point: Vec3): void;
  AddAngularImpulse(bodyID: number, angularImpulse: Vec3): void;
  AddLinearVelocity(bodyID: number, linearVelocity: Vec3): void;
  SetLinearAndAngularVelocity(bodyID: number, linearVelocity: Vec3, angularVelocity: Vec3): void;
  GetPointVelocity(out: Vec3, bodyID: number, point: Vec3): Vec3;
  NotifyShapeChanged(bodyID: number, prevCenterOfMass: Vec3, updateMassProperties: boolean, activationMode: EActivation): void;
  AddLinearAndAngularVelocity(bodyID: number, linearVelocity: Vec3, angularVelocity: Vec3): void;
  SetRotation(bodyID: number, rotation: Quat, activationMode: EActivation): void;
  SetPositionAndRotation(bodyID: number, position: Vec3, rotation: Quat, activationMode: EActivation): void;
  SetPositionRotationAndVelocity(bodyID: number, position: Vec3, rotation: Quat, linearVelocity: Vec3, angularVelocity: Vec3): void;
  SetPositionAndRotationWhenChanged(bodyID: number, position: Vec3, rotation: Quat, activationMode: EActivation): void;
  MoveKinematic(bodyID: number, targetPosition: Vec3, targetRotation: Quat, deltaTime: number): void;
  GetFriction(bodyID: number): number;
  GetRestitution(bodyID: number): number;
  GetGravityFactor(bodyID: number): number;
  SetFriction(bodyID: number, friction: number): void;
  SetRestitution(bodyID: number, restitution: number): void;
  SetGravityFactor(bodyID: number, gravityFactor: number): void;
  SetMaxLinearVelocity(bodyID: number, linearVelocity: number): void;
  GetMaxLinearVelocity(bodyID: number): number;
  SetMaxAngularVelocity(bodyID: number, angularVelocity: number): void;
  GetMaxAngularVelocity(bodyID: number): number;
  ApplyBuoyancyImpulse(bodyID: number, surfacePosition: Vec3, surfaceNormal: Vec3, buoyancy: number, linearDrag: number, angularDrag: number, fluidVelocity: Vec3, gravity: Vec3, deltaTime: number): boolean;
}

export interface Body extends ClassHandle {
  GetBodyType(): EBodyType;
  GetMotionType(): EMotionType;
  GetShape(): Shape | null;
  GetTransformedShape(): TransformedShape;
  GetBodyCreationSettings(): BodyCreationSettings;
  GetSoftBodyCreationSettings(): SoftBodyCreationSettings;
  GetSoftBodyMotionProperties(): SoftBodyMotionProperties | null;
  GetMotionProperties(): MotionProperties | null;
  GetCollisionGroup(): CollisionGroup | null;
  SetMotionType(motionType: EMotionType): void;
  ResetSleepTimer(): void;
  ResetForce(): void;
  ResetTorque(): void;
  ResetMotion(): void;
  SaveState(stream: StateRecorder): void;
  RestoreState(stream: StateRecorder): void;
  IsRigidBody(): boolean;
  IsSoftBody(): boolean;
  IsActive(): boolean;
  IsStatic(): boolean;
  IsKinematic(): boolean;
  IsDynamic(): boolean;
  IsSensor(): boolean;
  SetIsSensor(isSensor: boolean): void;
  GetAllowSleeping(): boolean;
  SetAllowSleeping(allowSleeping: boolean): void;
  CanBeKinematicOrDynamic(): boolean;
  IsInBroadPhase(): boolean;
  SetCollideKinematicVsNonDynamic(collide: boolean): void;
  GetCollideKinematicVsNonDynamic(): boolean;
  SetUseManifoldReduction(useReduction: boolean): void;
  GetUseManifoldReduction(): boolean;
  SetApplyGyroscopicForce(apply: boolean): void;
  GetApplyGyroscopicForce(): boolean;
  SetEnhancedInternalEdgeRemoval(apply: boolean): void;
  GetEnhancedInternalEdgeRemoval(): boolean;
  GetID(): number;
  GetObjectLayer(): number;
  GetPosition(out: Vec3): Vec3;
  GetRotation(out: Quat): Quat;
  GetCenterOfMassPosition(out: Vec3): Vec3;
  GetWorldTransform(out: Mat44): Mat44;
  GetCenterOfMassTransform(out: Mat44): Mat44;
  GetLinearVelocity(out: Vec3): Vec3;
  GetAngularVelocity(out: Vec3): Vec3;
  GetWorldSpaceBounds(out: AABox): AABox;
  GetAccumulatedForce(out: Vec3): Vec3;
  GetAccumulatedTorque(out: Vec3): Vec3;
  GetInverseInertia(out: Mat44): Mat44;
  GetInverseCenterOfMassTransform(out: Mat44): Mat44;
  GetUserData(): bigint;
  SetUserData(userData: bigint): void;
  GetPointVelocity(out: Vec3, point: Vec3): Vec3;
  SetLinearVelocity(linearVelocity: Vec3): void;
  SetAngularVelocity(angularVelocity: Vec3): void;
  AddForce(force: Vec3): void;
  AddForce(force: Vec3, point: Vec3): void;
  AddTorque(torque: Vec3): void;
  AddImpulse(impulse: Vec3): void;
  AddImpulse(impulse: Vec3, point: Vec3): void;
  SetLinearVelocityClamped(linearVelocity: Vec3): void;
  SetAngularVelocityClamped(angularVelocity: Vec3): void;
  AddAngularImpulse(angularImpulse: Vec3): void;
  GetWorldSpaceSurfaceNormal(out: Vec3, subShapeID: number, position: Vec3): Vec3;
  ApplyBuoyancyImpulse(surfacePosition: Vec3, surfaceNormal: Vec3, buoyancy: number, linearDrag: number, angularDrag: number, fluidVelocity: Vec3, gravity: Vec3, deltaTime: number): boolean;
  GetFriction(): number;
  GetRestitution(): number;
  SetFriction(friction: number): void;
  SetRestitution(restitution: number): void;
  MoveKinematic(targetPosition: Vec3, targetRotation: Quat, deltaTime: number): void;
  GetSoftBodyVertices(): Float32Array;
}

export interface MotionProperties extends ClassHandle {
  GetMotionQuality(): EMotionQuality;
  GetAllowedDOFs(): EAllowedDOFs;
  ClampLinearVelocity(): void;
  ClampAngularVelocity(): void;
  SetMassProperties(allowedDOFs: EAllowedDOFs, massProperties: MassProperties): void;
  ResetForce(): void;
  ResetTorque(): void;
  ResetMotion(): void;
  GetAllowSleeping(): boolean;
  SetNumVelocityStepsOverride(n: number): void;
  GetNumVelocityStepsOverride(): number;
  SetNumPositionStepsOverride(n: number): void;
  GetNumPositionStepsOverride(): number;
  GetLinearVelocity(out: Vec3): Vec3;
  GetAngularVelocity(out: Vec3): Vec3;
  GetInverseInertiaDiagonal(out: Vec3): Vec3;
  GetInertiaRotation(out: Quat): Quat;
  GetLocalSpaceInverseInertia(out: Mat44): Mat44;
  GetAccumulatedForce(out: Vec3): Vec3;
  GetAccumulatedTorque(out: Vec3): Vec3;
  SetLinearVelocity(linearVelocity: Vec3): void;
  SetAngularVelocity(angularVelocity: Vec3): void;
  SetLinearVelocityClamped(linearVelocity: Vec3): void;
  SetAngularVelocityClamped(angularVelocity: Vec3): void;
  GetPointVelocityCOM(out: Vec3, pointRelativeToCOM: Vec3): Vec3;
  LockTranslation(out: Vec3, v: Vec3): Vec3;
  LockAngular(out: Vec3, v: Vec3): Vec3;
  SetInverseInertia(diagonal: Vec3, rotation: Quat): void;
  MultiplyWorldSpaceInverseInertiaByVector(out: Vec3, rotation: Quat, v: Vec3): Vec3;
  GetInverseInertiaForRotation(out: Mat44, rotation: Mat44): Mat44;
  GetMaxLinearVelocity(): number;
  GetMaxAngularVelocity(): number;
  GetLinearDamping(): number;
  GetAngularDamping(): number;
  GetGravityFactor(): number;
  GetInverseMass(): number;
  GetInverseMassUnchecked(): number;
  SetMaxLinearVelocity(maxLinearVelocity: number): void;
  SetMaxAngularVelocity(maxAngularVelocity: number): void;
  SetLinearDamping(linearDamping: number): void;
  SetAngularDamping(angularDamping: number): void;
  SetGravityFactor(gravityFactor: number): void;
  SetInverseMass(inverseMass: number): void;
  MoveKinematic(deltaPosition: Vec3, deltaRotation: Quat, deltaTime: number): void;
  ScaleToMass(mass: number): void;
}

export interface SoftBodyMotionProperties extends MotionProperties {
  GetSettings(): SoftBodySharedSettings | null;
  GetEnableSkinConstraints(): boolean;
  SetEnableSkinConstraints(enable: boolean): void;
  GetNumVertices(): number;
  GetVertex(index: number): SoftBodyVertex | null;
  GetNumFaces(): number;
  GetFace(index: number): SoftBodySharedSettingsFace | null;
  GetNumMaterials(): number;
  GetMaterial(index: number): PhysicsMaterial | null;
  GetNumIterations(): number;
  SetNumIterations(n: number): void;
  GetLocalBounds(out: AABox): AABox;
  SkinVertices(rootTransform: Mat44, jointMatricesPtr: number, numJoints: number, hardSkinAll: boolean, jolt: JoltInterface): void;
  GetPressure(): number;
  SetPressure(pressure: number): void;
  GetSkinnedMaxDistanceMultiplier(): number;
  SetSkinnedMaxDistanceMultiplier(m: number): void;
  GetVolume(): number;
  CustomUpdate(deltaTime: number, softBody: Body, system: PhysicsSystem): void;
}

export interface ContactManifold extends ClassHandle {
  mBaseOffset: Vec3;
  mWorldSpaceNormal: Vec3;
  mPenetrationDepth: number;
  GetPointCount(): number;
  GetSubShapeID1(): number;
  GetSubShapeID2(): number;
  GetWorldSpaceContactPointOn1(out: Vec3, index: number): Vec3;
  GetWorldSpaceContactPointOn2(out: Vec3, index: number): Vec3;
}

export interface ContactSettings extends ClassHandle {
  mIsSensor: boolean;
  mRelativeLinearSurfaceVelocity: Vec3;
  mRelativeAngularSurfaceVelocity: Vec3;
  mCombinedFriction: number;
  mCombinedRestitution: number;
  mInvMassScale1: number;
  mInvInertiaScale1: number;
  mInvMassScale2: number;
  mInvInertiaScale2: number;
}

export interface ContactListener extends ClassHandle {
}

export interface ContactListenerWrapper extends ContactListener {
  notifyOnDestruction(): void;
}

export interface ThreadedContactListener extends ContactListener {
}

export interface BodyActivationListener extends ClassHandle {
}

export interface BodyActivationListenerWrapper extends BodyActivationListener {
  notifyOnDestruction(): void;
}

export interface ContactListenerBuffer extends ContactListener {
  Clear(): void;
  GetAddedCount(): number;
  GetPersistedCount(): number;
  GetRemovedCount(): number;
  AddedI32Ptr(): number;
  AddedF32Ptr(): number;
  PersistedI32Ptr(): number;
  PersistedF32Ptr(): number;
  PointsF32Ptr(): number;
  RemovedI32Ptr(): number;
}

export interface ActiveBodyBuffer extends ClassHandle {
  Refresh(physicsSystem: PhysicsSystem | null): void;
  GetBodyCount(): number;
  BodiesF32Ptr(): number;
}

export interface ArrayVec3 extends ClassHandle {
  clear(): void;
  empty(): boolean;
  size(): number;
  reserve(count: number): void;
  at(index: number): Vec3;
  push_back(value: Vec3): void;
}

export interface BodyLockInterface extends ClassHandle {
  TryGetBody(bodyID: number): Body | null;
}

export interface PhysicsSystem extends ClassHandle {
  GetBodyInterface(): BodyInterface | null;
  GetBodyLockInterface(): BodyLockInterface | null;
  GetBodyLockInterfaceNoLock(): BodyLockInterface | null;
  GetContactListener(): ContactListener | null;
  GetBodyActivationListener(): BodyActivationListener | null;
  GetBodyInterfaceNoLock(): BodyInterface | null;
  GetSoftBodyContactListener(): SoftBodyContactListener | null;
  GetPhysicsSettings(): PhysicsSettings | null;
  GetSimShapeFilter(): SimShapeFilter | null;
  GetNarrowPhaseQuery(): NarrowPhaseQuery | null;
  GetNarrowPhaseQueryNoLock(): NarrowPhaseQuery | null;
  GetBroadPhaseQuery(): BroadPhaseQuery | null;
  SetContactListener(listener: ContactListener | null): void;
  SetBodyActivationListener(listener: BodyActivationListener | null): void;
  AddConstraint(constraint: Constraint | null): void;
  RemoveConstraint(constraint: Constraint | null): void;
  OptimizeBroadPhase(): void;
  SaveState(stateRecorder: StateRecorder): void;
  SetPhysicsSettings(settings: PhysicsSettings): void;
  AddStepListener(listener: PhysicsStepListener | null): void;
  RemoveStepListener(listener: PhysicsStepListener | null): void;
  SetSimShapeFilter(filter: SimShapeFilter | null): void;
  SaveStateWithFilter(stateRecorder: StateRecorder, state: EStateRecorderState, filter: StateRecorderFilter | null): void;
  SetSoftBodyContactListener(listener: SoftBodyContactListener | null): void;
  RestoreState(stateRecorder: StateRecorder): boolean;
  RestoreStateWithFilter(stateRecorder: StateRecorder, filter: StateRecorderFilter | null): boolean;
  GetNumBodies(): number;
  GetNumActiveBodies(): number;
  GetMaxBodies(): number;
  WereBodiesInContact(bodyID1: number, bodyID2: number): boolean;
  GetGravity(out: Vec3): Vec3;
  GetRayHitNormal(out: Vec3, ray: RRayCast, hit: RayCastResult): Vec3;
  GetBounds(out: AABox): AABox;
  SetGravity(gravity: Vec3): void;
  GetBodies(): number[];
  GetActiveBodies(): number[];
}

export interface PhysicsSettings extends ClassHandle {
  mDeterministicSimulation: boolean;
  mConstraintWarmStart: boolean;
  mUseBodyPairContactCache: boolean;
  mUseManifoldReduction: boolean;
  mUseLargeIslandSplitter: boolean;
  mAllowSleeping: boolean;
  mCheckActiveEdges: boolean;
  mMaxInFlightBodyPairs: number;
  mStepListenersBatchSize: number;
  mStepListenerBatchesPerJob: number;
  mNumVelocitySteps: number;
  mNumPositionSteps: number;
  mBaumgarte: number;
  mSpeculativeContactDistance: number;
  mPenetrationSlop: number;
  mLinearCastThreshold: number;
  mLinearCastMaxPenetration: number;
  mMaxPenetrationDistance: number;
  mMinVelocityForRestitution: number;
  mTimeBeforeSleep: number;
  mPointVelocitySleepThreshold: number;
  mManifoldTolerance: number;
  mBodyPairCacheMaxDeltaPositionSq: number;
  mBodyPairCacheCosMaxDeltaRotationDiv2: number;
  mContactNormalCosMaxDeltaRotation: number;
  mContactPointPreserveLambdaMaxDistSq: number;
}

export interface PhysicsStepListener extends ClassHandle {
}

export interface PhysicsStepListenerWrapper extends PhysicsStepListener {
  notifyOnDestruction(): void;
}

export interface RRayCast extends ClassHandle {
  Set(origin: Vec3, direction: Vec3): void;
  GetPointOnRay(out: Vec3, fraction: number): Vec3;
}

export interface RayCast extends ClassHandle {
  GetOrigin(out: Vec3): Vec3;
  GetDirection(out: Vec3): Vec3;
  Set(origin: Vec3, direction: Vec3): void;
  GetPointOnRay(out: Vec3, fraction: number): Vec3;
}

export interface OrientedBox extends ClassHandle {
  GetOrientation(out: Mat44): Mat44;
  GetHalfExtents(out: Vec3): Vec3;
  SetHalfExtents(halfExtents: Vec3): void;
  SetOrientation(orientation: Mat44): void;
}

export interface AABoxCast extends ClassHandle {
  GetBox(out: AABox): AABox;
  GetDirection(out: Vec3): Vec3;
  SetDirection(direction: Vec3): void;
  SetBox(box: AABox): void;
}

export interface BroadPhaseCastResult extends ClassHandle {
  mFraction: number;
  Reset(): void;
  GetBodyID(): number;
}

export interface RayCastResult extends BroadPhaseCastResult {
  GetSubShapeID2(): number;
}

export interface BroadPhaseLayerFilter extends ClassHandle {
}

export interface BroadPhaseLayerFilterWrapper extends BroadPhaseLayerFilter {
  notifyOnDestruction(): void;
}

export interface ObjectLayerFilter extends ClassHandle {
}

export interface ObjectLayerFilterWrapper extends ObjectLayerFilter {
  notifyOnDestruction(): void;
}

export interface BodyFilter extends ClassHandle {
}

export interface BodyFilterWrapper extends BodyFilter {
  notifyOnDestruction(): void;
}

export interface SpecifiedBroadPhaseLayerFilter extends BroadPhaseLayerFilter {
}

export interface SpecifiedObjectLayerFilter extends ObjectLayerFilter {
}

export interface IgnoreSingleBodyFilter extends BodyFilter {
}

export interface IgnoreMultipleBodiesFilter extends BodyFilter {
  Clear(): void;
  Reserve(size: number): void;
  IgnoreBody(bodyID: number): void;
}

export interface ShapeFilter extends ClassHandle {
}

export interface ShapeFilterWrapper extends ShapeFilter {
  notifyOnDestruction(): void;
}

export interface SimShapeFilter extends ClassHandle {
}

export interface SimShapeFilterWrapper extends SimShapeFilter {
  notifyOnDestruction(): void;
}

export interface DefaultBroadPhaseLayerFilter extends BroadPhaseLayerFilter {
}

export interface DefaultObjectLayerFilter extends ObjectLayerFilter {
}

export interface NarrowPhaseQuery extends ClassHandle {
  CastRayCollide(ray: RRayCast, settings: RayCastSettings, collector: CastRayCollector): void;
  CastRayCollideWithFilters(ray: RRayCast, settings: RayCastSettings, collector: CastRayCollector, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter): void;
  CastRay(ray: RRayCast, hit: RayCastResult): boolean;
  CastRayWithFilters(ray: RRayCast, hit: RayCastResult, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter): boolean;
  CastShape(shapeCast: RShapeCast, settings: ShapeCastSettings, baseOffset: Vec3, collector: CastShapeCollector): void;
  CastShapeWithFilters(shapeCast: RShapeCast, settings: ShapeCastSettings, baseOffset: Vec3, collector: CastShapeCollector, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter): void;
  CollidePoint(point: Vec3, collector: CollidePointCollector): void;
  CollidePointWithFilters(point: Vec3, collector: CollidePointCollector, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter): void;
  CollectTransformedShapes(box: AABox, collector: TransformedShapeCollector): void;
  CollectTransformedShapesWithFilters(box: AABox, collector: TransformedShapeCollector, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter): void;
  CollideShape(shape: Shape | null, scale: Vec3, comTransform: Mat44, settings: CollideShapeSettings, baseOffset: Vec3, collector: CollideShapeCollector): void;
  CollideShapeWithFilters(shape: Shape | null, scale: Vec3, comTransform: Mat44, settings: CollideShapeSettings, baseOffset: Vec3, collector: CollideShapeCollector, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter): void;
  CollideShapeWithInternalEdgeRemoval(shape: Shape | null, scale: Vec3, comTransform: Mat44, settings: CollideShapeSettings, baseOffset: Vec3, collector: CollideShapeCollector): void;
  CollideShapeWithInternalEdgeRemovalWithFilters(shape: Shape | null, scale: Vec3, comTransform: Mat44, settings: CollideShapeSettings, baseOffset: Vec3, collector: CollideShapeCollector, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter): void;
  CastRayAll(ray: RRayCast, objectLayer: number, jolt: JoltInterface): Array<{ bodyID: number; fraction: number; point: Vec3; normal: Vec3 }>;
}

export interface BroadPhaseQuery extends ClassHandle {
  CastRay(ray: RayCast, collector: RayCastBodyCollector, broadPhaseLayerFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter): void;
  CollideOrientedBox(box: OrientedBox, collector: CollideShapeBodyCollector, broadPhaseLayerFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter): void;
  CastAABox(box: AABoxCast, collector: CastShapeBodyCollector, broadPhaseLayerFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter): void;
  GetBounds(out: AABox): AABox;
  CollidePoint(point: Vec3, collector: CollideShapeBodyCollector, broadPhaseLayerFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter): void;
  CollideAABox(box: AABox, collector: CollideShapeBodyCollector, broadPhaseLayerFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter): void;
  CollideSphere(center: Vec3, radius: number, collector: CollideShapeBodyCollector, broadPhaseLayerFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter): void;
}

export interface CollideShapeResult extends ClassHandle {
  mContactPointOn1: Vec3;
  mContactPointOn2: Vec3;
  mPenetrationAxis: Vec3;
  mPenetrationDepth: number;
  GetBodyID2(): number;
  GetSubShapeID1(): number;
  GetSubShapeID2(): number;
  GetShape1FaceCount(): number;
  GetShape2FaceCount(): number;
  GetShape1FaceVertex(out: Vec3, index: number): Vec3;
  GetShape2FaceVertex(out: Vec3, index: number): Vec3;
}

export interface ShapeCastResult extends CollideShapeResult {
  mIsBackFaceHit: boolean;
  mFraction: number;
}

export interface CollidePointResult extends ClassHandle {
  GetBodyID(): number;
}

export interface RayCastSettings extends ClassHandle {
  mBackFaceModeTriangles: EBackFaceMode;
  mBackFaceModeConvex: EBackFaceMode;
  mTreatConvexAsSolid: boolean;
  SetBackFaceMode(mode: EBackFaceMode): void;
}

export interface CollideShapeSettings extends ClassHandle {
  mBackFaceMode: EBackFaceMode;
  mActiveEdgeMode: EActiveEdgeMode;
  mCollectFacesMode: ECollectFacesMode;
  mMaxSeparationDistance: number;
  mCollisionTolerance: number;
  mPenetrationTolerance: number;
  GetActiveEdgeMovementDirection(out: Vec3): Vec3;
  SetActiveEdgeMovementDirection(direction: Vec3): void;
}

export interface ShapeCastSettings extends ClassHandle {
  mBackFaceModeTriangles: EBackFaceMode;
  mBackFaceModeConvex: EBackFaceMode;
  mActiveEdgeMode: EActiveEdgeMode;
  mCollectFacesMode: ECollectFacesMode;
  mReturnDeepestPoint: boolean;
  mUseShrunkenShapeAndConvexRadius: boolean;
  mCollisionTolerance: number;
  mPenetrationTolerance: number;
  GetActiveEdgeMovementDirection(out: Vec3): Vec3;
  SetActiveEdgeMovementDirection(direction: Vec3): void;
}

export interface RShapeCast extends ClassHandle {
}

export interface CastRayCollector extends ClassHandle {
}

export interface CastRayCollectorWrapper extends CastRayCollector {
  notifyOnDestruction(): void;
}

export interface TransformedShapeCollector extends ClassHandle {
}

export interface TransformedShapeCollectorWrapper extends TransformedShapeCollector {
  notifyOnDestruction(): void;
}

export interface CollideShapeCollector extends ClassHandle {
}

export interface CollideShapeCollectorWrapper extends CollideShapeCollector {
  notifyOnDestruction(): void;
}

export interface CastShapeCollector extends ClassHandle {
}

export interface CastShapeCollectorWrapper extends CastShapeCollector {
  notifyOnDestruction(): void;
}

export interface CollidePointCollector extends ClassHandle {
}

export interface CollidePointCollectorWrapper extends CollidePointCollector {
  notifyOnDestruction(): void;
}

export interface RayCastBodyCollector extends ClassHandle {
}

export interface RayCastBodyCollectorWrapper extends RayCastBodyCollector {
  notifyOnDestruction(): void;
}

export interface CastShapeBodyCollector extends ClassHandle {
}

export interface CastShapeBodyCollectorWrapper extends CastShapeBodyCollector {
  notifyOnDestruction(): void;
}

export interface CollideShapeBodyCollector extends ClassHandle {
}

export interface CollideShapeBodyCollectorWrapper extends CollideShapeBodyCollector {
  notifyOnDestruction(): void;
}

export interface CollideShapeClosestHitCollector extends CollideShapeCollector {
  GetHit(): CollideShapeResult | null;
  Reset(): void;
  HadHit(): boolean;
}

export interface CollideShapeAllHitCollector extends CollideShapeCollector {
  Reset(): void;
  Sort(): void;
  HadHit(): boolean;
  size(): number;
  at(index: number): CollideShapeResult | null;
}

export interface CastShapeClosestHitCollector extends CastShapeCollector {
  GetHit(): ShapeCastResult | null;
  Reset(): void;
  HadHit(): boolean;
}

export interface CastShapeAllHitCollector extends CastShapeCollector {
  Reset(): void;
  Sort(): void;
  HadHit(): boolean;
  size(): number;
  at(index: number): ShapeCastResult | null;
}

export interface CollidePointClosestHitCollector extends CollidePointCollector {
  GetHit(): CollidePointResult | null;
  Reset(): void;
  HadHit(): boolean;
}

export interface CollidePointAllHitCollector extends CollidePointCollector {
  Reset(): void;
  Sort(): void;
  HadHit(): boolean;
  size(): number;
  at(index: number): CollidePointResult | null;
}

export interface CastRayClosestHitCollector extends CastRayCollector {
  GetHit(): RayCastResult | null;
  Reset(): void;
  HadHit(): boolean;
}

export interface CastRayAllHitCollector extends CastRayCollector {
  Reset(): void;
  Sort(): void;
  HadHit(): boolean;
  size(): number;
  at(index: number): RayCastResult | null;
}

export interface RayCastBodyClosestHitCollector extends RayCastBodyCollector {
  GetHit(): BroadPhaseCastResult | null;
  Reset(): void;
  HadHit(): boolean;
}

export interface RayCastBodyAllHitCollector extends RayCastBodyCollector {
  Reset(): void;
  Sort(): void;
  HadHit(): boolean;
  size(): number;
  at(index: number): BroadPhaseCastResult | null;
}

export interface CastShapeBodyClosestHitCollector extends CastShapeBodyCollector {
  GetHit(): BroadPhaseCastResult | null;
  Reset(): void;
  HadHit(): boolean;
}

export interface CastShapeBodyAllHitCollector extends CastShapeBodyCollector {
  Reset(): void;
  Sort(): void;
  HadHit(): boolean;
  size(): number;
  at(index: number): BroadPhaseCastResult | null;
}

export interface PhysicsMaterial extends ClassHandle {
  GetDebugColor(): Color;
  GetDebugName(): string;
}

export interface PhysicsMaterialSimple extends PhysicsMaterial {
}

export interface GroupFilter extends ClassHandle {
}

export interface GroupFilterJS extends GroupFilter {
  notifyOnDestruction(): void;
}

export interface GroupFilterTable extends GroupFilter {
  DisableCollision(subGroup1: number, subGroup2: number): void;
  EnableCollision(subGroup1: number, subGroup2: number): void;
  IsCollisionEnabled(subGroup1: number, subGroup2: number): boolean;
}

export interface CollisionGroup extends ClassHandle {
  GetGroupFilter(): GroupFilter | null;
  SetGroupFilter(filter: GroupFilter | null): void;
  SetGroupID(id: number): void;
  GetGroupID(): number;
  SetSubGroupID(id: number): void;
  GetSubGroupID(): number;
}

export interface MassProperties extends ClassHandle {
  mInertia: Mat44;
  mMass: number;
  DecomposePrincipalMomentsOfInertiaInto(outRotation: number, outDiagonal: number): boolean;
  Translate(translation: Vec3): void;
  Scale(scale: Vec3): void;
  Rotate(rotation: Mat44): void;
  SetMassAndInertiaOfSolidBox(boxSize: Vec3, density: number): void;
  ScaleToMass(mass: number): void;
}

export interface SpringSettings extends ClassHandle {
  mMode: ESpringMode;
  mFrequency: number;
  mStiffness: number;
  mDamping: number;
  HasStiffness(): boolean;
}

export interface LinearCurvePoint extends ClassHandle {
  mX: number;
  mY: number;
}

export interface LinearCurve extends ClassHandle {
  Clear(): void;
  Sort(): void;
  Reserve(size: number): void;
  GetNumPoints(): number;
  GetPoint(index: number): LinearCurvePoint | null;
  AddPoint(x: number, y: number): void;
  GetMinX(): number;
  GetMaxX(): number;
  GetValue(x: number): number;
}

export interface MotorSettings extends ClassHandle {
  mMinForceLimit: number;
  mMaxForceLimit: number;
  mMinTorqueLimit: number;
  mMaxTorqueLimit: number;
  GetSpringSettings(): SpringSettings | null;
  SetForceLimit(limit: number): void;
  SetTorqueLimit(limit: number): void;
  SetForceLimits(min: number, max: number): void;
  SetTorqueLimits(min: number, max: number): void;
}

export interface Constraint extends ClassHandle {
  GetType(): EConstraintType;
  GetSubType(): EConstraintSubType;
  GetConstraintSettings(): ConstraintSettings | null;
  ResetWarmStart(): void;
  SetEnabled(enabled: boolean): void;
  GetEnabled(): boolean;
  IsActive(): boolean;
  GetConstraintPriority(): number;
  SetConstraintPriority(priority: number): void;
  GetNumVelocityStepsOverride(): number;
  SetNumVelocityStepsOverride(n: number): void;
  GetNumPositionStepsOverride(): number;
  SetNumPositionStepsOverride(n: number): void;
  GetUserData(): bigint;
  SetUserData(userData: bigint): void;
}

export interface TwoBodyConstraint extends Constraint {
  GetBody1(): Body | null;
  GetBody2(): Body | null;
  GetConstraintToBody1Matrix(out: Mat44): Mat44;
  GetConstraintToBody2Matrix(out: Mat44): Mat44;
}

export interface ConstraintSettings extends ClassHandle {
  mEnabled: boolean;
  mConstraintPriority: number;
  mNumVelocityStepsOverride: number;
  mNumPositionStepsOverride: number;
  mUserData: bigint;
  mDrawConstraintSize: number;
}

export interface TwoBodyConstraintSettings extends ConstraintSettings {
  Create(body1: Body, body2: Body): TwoBodyConstraint | null;
}

export interface FixedConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mAutoDetectPoint: boolean;
  mPoint1: Vec3;
  mAxisX1: Vec3;
  mAxisY1: Vec3;
  mPoint2: Vec3;
  mAxisX2: Vec3;
  mAxisY2: Vec3;
}

export interface FixedConstraint extends TwoBodyConstraint {
  GetTotalLambdaPosition(out: Vec3): Vec3;
  GetTotalLambdaRotation(out: Vec3): Vec3;
}

export interface PointConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mPoint1: Vec3;
  mPoint2: Vec3;
}

export interface PointConstraint extends TwoBodyConstraint {
  GetLocalSpacePoint1(out: Vec3): Vec3;
  GetLocalSpacePoint2(out: Vec3): Vec3;
  GetTotalLambdaPosition(out: Vec3): Vec3;
  SetPoint1(space: EConstraintSpace, point: Vec3): void;
  SetPoint2(space: EConstraintSpace, point: Vec3): void;
}

export interface DistanceConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mPoint1: Vec3;
  mPoint2: Vec3;
  mMinDistance: number;
  mMaxDistance: number;
}

export interface DistanceConstraint extends TwoBodyConstraint {
  GetLimitsSpringSettings(): SpringSettings | null;
  SetLimitsSpringSettings(settings: SpringSettings): void;
  SetDistance(min: number, max: number): void;
  GetMinDistance(): number;
  GetMaxDistance(): number;
  GetTotalLambdaPosition(): number;
}

export interface HingeConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mPoint1: Vec3;
  mHingeAxis1: Vec3;
  mNormalAxis1: Vec3;
  mPoint2: Vec3;
  mHingeAxis2: Vec3;
  mNormalAxis2: Vec3;
  mLimitsMin: number;
  mLimitsMax: number;
  mMaxFrictionTorque: number;
  GetMotorSettings(): MotorSettings | null;
}

export interface HingeConstraint extends TwoBodyConstraint {
  GetMotorState(): EMotorState;
  GetMotorSettings(): MotorSettings | null;
  GetLimitsSpringSettings(): SpringSettings | null;
  SetMotorState(state: EMotorState): void;
  SetLimitsSpringSettings(settings: SpringSettings): void;
  HasLimits(): boolean;
  GetLocalSpacePoint1(out: Vec3): Vec3;
  GetLocalSpacePoint2(out: Vec3): Vec3;
  GetLocalSpaceHingeAxis1(out: Vec3): Vec3;
  GetLocalSpaceHingeAxis2(out: Vec3): Vec3;
  GetLocalSpaceNormalAxis1(out: Vec3): Vec3;
  GetLocalSpaceNormalAxis2(out: Vec3): Vec3;
  GetTotalLambdaPosition(out: Vec3): Vec3;
  GetTotalLambdaRotation(out: Vec3): Vec3;
  SetTargetOrientationBS(orientation: Quat): void;
  GetCurrentAngle(): number;
  SetTargetAngularVelocity(velocity: number): void;
  GetTargetAngularVelocity(): number;
  SetTargetAngle(angle: number): void;
  GetTargetAngle(): number;
  SetLimits(min: number, max: number): void;
  GetLimitsMin(): number;
  GetLimitsMax(): number;
  SetMaxFrictionTorque(frictionTorque: number): void;
  GetMaxFrictionTorque(): number;
  GetTotalLambdaRotationLimits(): number;
  GetTotalLambdaMotor(): number;
}

export interface ConeConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mPoint1: Vec3;
  mTwistAxis1: Vec3;
  mPoint2: Vec3;
  mTwistAxis2: Vec3;
  mHalfConeAngle: number;
}

export interface ConeConstraint extends TwoBodyConstraint {
  GetTotalLambdaPosition(out: Vec3): Vec3;
  SetHalfConeAngle(angle: number): void;
  GetCosHalfConeAngle(): number;
  GetTotalLambdaRotation(): number;
}

export interface SliderConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mAutoDetectPoint: boolean;
  mPoint1: Vec3;
  mSliderAxis1: Vec3;
  mNormalAxis1: Vec3;
  mPoint2: Vec3;
  mSliderAxis2: Vec3;
  mNormalAxis2: Vec3;
  mLimitsMin: number;
  mLimitsMax: number;
  mMaxFrictionForce: number;
  GetMotorSettings(): MotorSettings | null;
  SetSliderAxis(sliderAxis: Vec3): void;
}

export interface SliderConstraint extends TwoBodyConstraint {
  GetMotorState(): EMotorState;
  GetMotorSettings(): MotorSettings | null;
  GetLimitsSpringSettings(): SpringSettings | null;
  SetMotorState(state: EMotorState): void;
  SetLimitsSpringSettings(settings: SpringSettings): void;
  HasLimits(): boolean;
  GetTotalLambdaPosition(out: Vec3): Vec3;
  GetTotalLambdaRotation(out: Vec3): Vec3;
  GetCurrentPosition(): number;
  SetTargetVelocity(velocity: number): void;
  GetTargetVelocity(): number;
  SetTargetPosition(position: number): void;
  GetTargetPosition(): number;
  SetLimits(min: number, max: number): void;
  GetLimitsMin(): number;
  GetLimitsMax(): number;
  SetMaxFrictionForce(frictionForce: number): void;
  GetMaxFrictionForce(): number;
  GetTotalLambdaPositionLimits(): number;
  GetTotalLambdaMotor(): number;
}

export interface SwingTwistConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mSwingType: ESwingType;
  mPosition1: Vec3;
  mTwistAxis1: Vec3;
  mPlaneAxis1: Vec3;
  mPosition2: Vec3;
  mTwistAxis2: Vec3;
  mPlaneAxis2: Vec3;
  mNormalHalfConeAngle: number;
  mPlaneHalfConeAngle: number;
  mTwistMinAngle: number;
  mTwistMaxAngle: number;
  mMaxFrictionTorque: number;
  GetSwingMotorSettings(): MotorSettings | null;
  GetTwistMotorSettings(): MotorSettings | null;
}

export interface SwingTwistConstraint extends TwoBodyConstraint {
  GetSwingMotorSettings(): MotorSettings | null;
  GetTwistMotorSettings(): MotorSettings | null;
  GetSwingMotorState(): EMotorState;
  GetTwistMotorState(): EMotorState;
  SetSwingMotorState(state: EMotorState): void;
  SetTwistMotorState(state: EMotorState): void;
  GetLocalSpacePosition1(out: Vec3): Vec3;
  GetLocalSpacePosition2(out: Vec3): Vec3;
  GetConstraintToBody1(out: Quat): Quat;
  GetConstraintToBody2(out: Quat): Quat;
  GetTargetAngularVelocityCS(out: Vec3): Vec3;
  GetTargetOrientationCS(out: Quat): Quat;
  GetRotationInConstraintSpace(out: Quat): Quat;
  GetTotalLambdaPosition(out: Vec3): Vec3;
  GetTotalLambdaMotor(out: Vec3): Vec3;
  SetTargetAngularVelocityCS(angularVelocity: Vec3): void;
  SetTargetOrientationCS(orientation: Quat): void;
  SetTargetOrientationBS(orientation: Quat): void;
  GetNormalHalfConeAngle(): number;
  SetNormalHalfConeAngle(angle: number): void;
  GetPlaneHalfConeAngle(): number;
  SetPlaneHalfConeAngle(angle: number): void;
  GetTwistMinAngle(): number;
  SetTwistMinAngle(angle: number): void;
  GetTwistMaxAngle(): number;
  SetTwistMaxAngle(angle: number): void;
  SetMaxFrictionTorque(frictionTorque: number): void;
  GetMaxFrictionTorque(): number;
  GetTotalLambdaTwist(): number;
  GetTotalLambdaSwingY(): number;
  GetTotalLambdaSwingZ(): number;
}

export interface SixDOFConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mSwingType: ESwingType;
  mPosition1: Vec3;
  mAxisX1: Vec3;
  mAxisY1: Vec3;
  mPosition2: Vec3;
  mAxisX2: Vec3;
  mAxisY2: Vec3;
  MakeFreeAxis(axis: SixDOFConstraintSettings_EAxis): void;
  MakeFixedAxis(axis: SixDOFConstraintSettings_EAxis): void;
  IsFreeAxis(axis: SixDOFConstraintSettings_EAxis): boolean;
  IsFixedAxis(axis: SixDOFConstraintSettings_EAxis): boolean;
  GetLimitsSpringSettings(axis: number): SpringSettings | null;
  GetMotorSettings(axis: number): MotorSettings | null;
  SetLimitedAxis(axis: SixDOFConstraintSettings_EAxis, min: number, max: number): void;
  GetLimitMin(axis: number): number;
  SetLimitMin(axis: number, v: number): void;
  GetLimitMax(axis: number): number;
  SetLimitMax(axis: number, v: number): void;
  GetMaxFriction(axis: number): number;
  SetMaxFriction(axis: number, v: number): void;
}

export interface SixDOFConstraint extends TwoBodyConstraint {
  GetLimitsSpringSettings(axis: SixDOFConstraintSettings_EAxis): SpringSettings | null;
  GetMotorSettings(axis: SixDOFConstraintSettings_EAxis): MotorSettings | null;
  GetMotorState(axis: SixDOFConstraintSettings_EAxis): EMotorState;
  SetMotorState(axis: SixDOFConstraintSettings_EAxis, state: EMotorState): void;
  SetLimitsSpringSettings(axis: SixDOFConstraintSettings_EAxis, settings: SpringSettings): void;
  IsFixedAxis(axis: SixDOFConstraintSettings_EAxis): boolean;
  IsFreeAxis(axis: SixDOFConstraintSettings_EAxis): boolean;
  GetTranslationLimitsMin(out: Vec3): Vec3;
  GetTranslationLimitsMax(out: Vec3): Vec3;
  GetRotationLimitsMin(out: Vec3): Vec3;
  GetRotationLimitsMax(out: Vec3): Vec3;
  GetRotationInConstraintSpace(out: Quat): Quat;
  GetTargetVelocityCS(out: Vec3): Vec3;
  GetTargetAngularVelocityCS(out: Vec3): Vec3;
  GetTargetPositionCS(out: Vec3): Vec3;
  GetTargetOrientationCS(out: Quat): Quat;
  GetTotalLambdaPosition(out: Vec3): Vec3;
  GetTotalLambdaRotation(out: Vec3): Vec3;
  GetTotalLambdaMotorTranslation(out: Vec3): Vec3;
  GetTotalLambdaMotorRotation(out: Vec3): Vec3;
  SetTargetPositionCS(position: Vec3): void;
  SetTranslationLimits(limitMin: Vec3, limitMax: Vec3): void;
  SetRotationLimits(limitMin: Vec3, limitMax: Vec3): void;
  SetTargetVelocityCS(velocity: Vec3): void;
  SetTargetAngularVelocityCS(angularVelocity: Vec3): void;
  SetTargetOrientationCS(orientation: Quat): void;
  SetTargetOrientationBS(orientation: Quat): void;
  GetLimitsMin(axis: SixDOFConstraintSettings_EAxis): number;
  GetLimitsMax(axis: SixDOFConstraintSettings_EAxis): number;
  SetMaxFriction(axis: SixDOFConstraintSettings_EAxis, friction: number): void;
  GetMaxFriction(axis: SixDOFConstraintSettings_EAxis): number;
}

export interface GearConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mHingeAxis1: Vec3;
  mHingeAxis2: Vec3;
  mRatio: number;
  SetRatio(numTeethGear1: number, numTeethGear2: number): void;
}

export interface GearConstraint extends TwoBodyConstraint {
  SetConstraints(gear1: Constraint | null, gear2: Constraint | null): void;
  GetTotalLambda(): number;
}

export interface RackAndPinionConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mHingeAxis: Vec3;
  mSliderAxis: Vec3;
  mRatio: number;
  SetRatio(numTeethRack: number, rackLength: number, numTeethPinion: number): void;
}

export interface RackAndPinionConstraint extends TwoBodyConstraint {
  SetConstraints(pinion: Constraint | null, rack: Constraint | null): void;
  GetTotalLambda(): number;
}

export interface PulleyConstraintSettings extends TwoBodyConstraintSettings {
  mSpace: EConstraintSpace;
  mBodyPoint1: Vec3;
  mFixedPoint1: Vec3;
  mBodyPoint2: Vec3;
  mFixedPoint2: Vec3;
  mRatio: number;
  mMinLength: number;
  mMaxLength: number;
}

export interface PulleyConstraint extends TwoBodyConstraint {
  SetLength(min: number, max: number): void;
  GetCurrentLength(): number;
  GetMinLength(): number;
  GetMaxLength(): number;
  GetTotalLambdaPosition(): number;
}

export interface PathConstraintPath extends ClassHandle {
  SetIsLooping(isLooping: boolean): void;
  IsLooping(): boolean;
  GetPathMaxFraction(): number;
  GetClosestPoint(position: Vec3, fractionHint: number): number;
}

export interface PathConstraintPathWrapper extends PathConstraintPath {
  notifyOnDestruction(): void;
}

export interface PathConstraintPathHermite extends PathConstraintPath {
  AddPoint(position: Vec3, tangent: Vec3, normal: Vec3): void;
}

export interface PathConstraintSettings extends TwoBodyConstraintSettings {
  mRotationConstraintType: EPathRotationConstraintType;
  mPathPosition: Vec3;
  mPathRotation: Quat;
  mPathFraction: number;
  mMaxFrictionForce: number;
  GetPositionMotorSettings(): MotorSettings | null;
  SetPath(path: PathConstraintPath | null, pathFraction: number): void;
}

export interface PathConstraint extends TwoBodyConstraint {
  GetPositionMotorState(): EMotorState;
  GetPositionMotorSettings(): MotorSettings | null;
  SetPositionMotorState(state: EMotorState): void;
  SetPath(path: PathConstraintPath | null, pathFraction: number): void;
  GetPathFraction(): number;
  SetMaxFrictionForce(frictionForce: number): void;
  GetMaxFrictionForce(): number;
  SetTargetVelocity(velocity: number): void;
  GetTargetVelocity(): number;
  SetTargetPathFraction(fraction: number): void;
  GetTargetPathFraction(): number;
}

export interface BroadPhaseLayer extends ClassHandle {
}

export interface BroadPhaseLayerInterface extends ClassHandle {
  GetNumBroadPhaseLayers(): number;
}

export interface BroadPhaseLayerInterfaceWrapper extends BroadPhaseLayerInterface {
  notifyOnDestruction(): void;
}

export interface ObjectVsBroadPhaseLayerFilter extends ClassHandle {
}

export interface ObjectVsBroadPhaseLayerFilterWrapper extends ObjectVsBroadPhaseLayerFilter {
  notifyOnDestruction(): void;
}

export interface ObjectLayerPairFilter extends ClassHandle {
}

export interface ObjectLayerPairFilterWrapper extends ObjectLayerPairFilter {
  notifyOnDestruction(): void;
}

export interface ObjectLayerPairFilterTable extends ObjectLayerPairFilter {
  GetNumObjectLayers(): number;
  EnableCollision(layer1: number, layer2: number): void;
  DisableCollision(layer1: number, layer2: number): void;
}

export interface BroadPhaseLayerInterfaceTable extends BroadPhaseLayerInterface {
  MapObjectToBroadPhaseLayer(objectLayer: number, broadPhaseLayer: BroadPhaseLayer): void;
}

export interface ObjectVsBroadPhaseLayerFilterTable extends ObjectVsBroadPhaseLayerFilter {
}

export interface ObjectLayerPairFilterMask extends ObjectLayerPairFilter {
}

export interface BroadPhaseLayerInterfaceMask extends BroadPhaseLayerInterface {
  ConfigureLayer(broadPhaseLayer: BroadPhaseLayer, groupsToInclude: number, groupsToExclude: number): void;
}

export interface ObjectVsBroadPhaseLayerFilterMask extends ObjectVsBroadPhaseLayerFilter {
}

export interface StateRecorder extends ClassHandle {
  SetValidating(validating: boolean): void;
  IsValidating(): boolean;
  SetIsLastPart(isLastPart: boolean): void;
  IsLastPart(): boolean;
}

export interface StateRecorderFilter extends ClassHandle {
}

export interface StateRecorderFilterWrapper extends StateRecorderFilter {
  notifyOnDestruction(): void;
}

export interface StateRecorderImpl extends StateRecorder {
  Rewind(): void;
  Clear(): void;
  IsEqual(reference: StateRecorderImpl): boolean;
}

export interface JoltSettings extends ClassHandle {
  mBroadPhaseLayerInterface: BroadPhaseLayerInterface | null;
  mObjectVsBroadPhaseLayerFilter: ObjectVsBroadPhaseLayerFilter | null;
  mObjectLayerPairFilter: ObjectLayerPairFilter | null;
  mMaxBodies: number;
  mMaxBodyPairs: number;
  mMaxContactConstraints: number;
}

export interface JoltInterface extends ClassHandle {
  GetPhysicsSystem(): PhysicsSystem | null;
  GetObjectLayerPairFilter(): ObjectLayerPairFilter | null;
  GetObjectVsBroadPhaseLayerFilter(): ObjectVsBroadPhaseLayerFilter | null;
  Step(deltaTime: number, collisionSteps: number): void;
}

export interface CharacterContactSettings extends ClassHandle {
  mCanPushCharacter: boolean;
  mCanReceiveImpulses: boolean;
}

export interface CharacterContactListener extends ClassHandle {
}

export interface CharacterContactListenerWrapper extends CharacterContactListener {
  notifyOnDestruction(): void;
}

export interface CharacterID extends ClassHandle {
  IsInvalid(): boolean;
  GetValue(): number;
}

export interface CharacterVsCharacterCollision extends ClassHandle {
}

export interface CharacterVsCharacterCollisionWrapper extends CharacterVsCharacterCollision {
  notifyOnDestruction(): void;
}

export interface CharacterVsCharacterCollisionSimple extends CharacterVsCharacterCollision {
  Add(character: CharacterVirtual | null): void;
  Remove(character: CharacterVirtual | null): void;
}

export interface CharacterVirtualContact extends ClassHandle {
  mMotionTypeB: EMotionType;
  mIsSensorB: boolean;
  mHadCollision: boolean;
  mWasDiscarded: boolean;
  mCanPushCharacter: boolean;
  mDistance: number;
  mFraction: number;
  GetMaterial(): PhysicsMaterial | null;
  GetCharacterB(): CharacterVirtual | null;
  IsSameBody(other: CharacterVirtualContact): boolean;
  GetBodyB(): number;
  GetCharacterIDB(): number;
  GetSubShapeIDB(): number;
  GetUserData(): number;
  GetPosition(out: Vec3): Vec3;
  GetLinearVelocity(out: Vec3): Vec3;
  GetContactNormal(out: Vec3): Vec3;
  GetSurfaceNormal(out: Vec3): Vec3;
}

export interface ArrayCharacterVirtualContact extends ClassHandle {
  empty(): boolean;
  size(): number;
  at(index: number): CharacterVirtualContact | null;
}

export interface CharacterBaseSettings extends ClassHandle {
  mEnhancedInternalEdgeRemoval: boolean;
  mUp: Vec3;
  mMaxSlopeAngle: number;
  SetShape(shape: Shape | null): void;
  SetSupportingVolume(normal: Vec3, constant: number): void;
}

export interface CharacterVirtualSettings extends CharacterBaseSettings {
  mBackFaceMode: EBackFaceMode;
  mID: CharacterID;
  mMaxCollisionIterations: number;
  mMaxConstraintIterations: number;
  mMaxNumHits: number;
  mInnerBodyLayer: number;
  mShapeOffset: Vec3;
  mMass: number;
  mMaxStrength: number;
  mPredictiveContactDistance: number;
  mCharacterPadding: number;
  mHitReductionCosMaxAngle: number;
  mPenetrationRecoverySpeed: number;
  mMinTimeRemaining: number;
  mCollisionTolerance: number;
  SetInnerBodyShape(shape: Shape | null): void;
  GetInnerBodyIDOverride(): number;
  SetInnerBodyIDOverride(bodyID: number): void;
}

export interface ExtendedUpdateSettings extends ClassHandle {
  mStickToFloorStepDown: Vec3;
  mWalkStairsStepUp: Vec3;
  mWalkStairsStepDownExtra: Vec3;
  mWalkStairsMinStepForward: number;
  mWalkStairsStepForwardTest: number;
  mWalkStairsCosAngleForwardContact: number;
}

export interface CharacterBase extends ClassHandle {
  GetShape(): Shape | null;
  GetGroundState(): EGroundState;
  GetGroundMaterial(): PhysicsMaterial | null;
  SaveState(stream: StateRecorder): void;
  RestoreState(stream: StateRecorder): void;
  IsSupported(): boolean;
  GetGroundBodyID(): number;
  GetGroundSubShapeID(): number;
  GetGroundUserData(): number;
  GetUp(out: Vec3): Vec3;
  GetGroundPosition(out: Vec3): Vec3;
  GetGroundNormal(out: Vec3): Vec3;
  GetGroundVelocity(out: Vec3): Vec3;
  SetUp(up: Vec3): void;
  IsSlopeTooSteep(normal: Vec3): boolean;
  GetCosMaxSlopeAngle(): number;
  SetMaxSlopeAngle(maxSlopeAngle: number): void;
}

export interface CharacterVirtual extends CharacterBase {
  GetID(): CharacterID;
  GetListener(): CharacterContactListener | null;
  GetTransformedShape(): TransformedShape;
  GetActiveContacts(): ArrayCharacterVirtualContact | null;
  SetListener(listener: CharacterContactListener | null): void;
  UpdateGroundVelocity(): void;
  SetCharacterVsCharacterCollision(collision: CharacterVsCharacterCollision | null): void;
  SetInnerBodyShape(shape: Shape | null): void;
  StartTrackingContactChanges(): void;
  FinishTrackingContactChanges(): void;
  GetMaxHitsExceeded(): boolean;
  GetEnhancedInternalEdgeRemoval(): boolean;
  SetEnhancedInternalEdgeRemoval(apply: boolean): void;
  HasCollidedWithCharacterID(characterID: CharacterID): boolean;
  HasCollidedWithCharacter(character: CharacterVirtual | null): boolean;
  RefreshContacts(objectLayer: number, jolt: JoltInterface): void;
  GetInnerBodyID(): number;
  GetUserData(): number;
  SetUserData(userData: number): void;
  GetMaxNumHits(): number;
  SetMaxNumHits(maxHits: number): void;
  HasCollidedWithBody(bodyID: number): boolean;
  GetPosition(out: Vec3): Vec3;
  GetRotation(out: Quat): Quat;
  GetLinearVelocity(out: Vec3): Vec3;
  GetCenterOfMassPosition(out: Vec3): Vec3;
  GetWorldTransform(out: Mat44): Mat44;
  GetCenterOfMassTransform(out: Mat44): Mat44;
  GetShapeOffset(out: Vec3): Vec3;
  SetPosition(position: Vec3): void;
  SetLinearVelocity(velocity: Vec3): void;
  CanWalkStairs(linearVelocity: Vec3): boolean;
  SetShapeOffset(shapeOffset: Vec3): void;
  CancelVelocityTowardsSteepSlopes(out: Vec3, desiredVelocity: Vec3): Vec3;
  StickToFloor(stepDown: Vec3, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter, jolt: JoltInterface): boolean;
  SetRotation(rotation: Quat): void;
  GetMass(): number;
  SetMass(mass: number): void;
  GetMaxStrength(): number;
  SetMaxStrength(maxStrength: number): void;
  GetPenetrationRecoverySpeed(): number;
  SetPenetrationRecoverySpeed(speed: number): void;
  GetCharacterPadding(): number;
  Update(deltaTime: number, gravity: Vec3, objectLayer: number, jolt: JoltInterface): void;
  ExtendedUpdate(deltaTime: number, gravity: Vec3, updateSettings: ExtendedUpdateSettings, objectLayer: number, jolt: JoltInterface): void;
  UpdateWithFilters(deltaTime: number, gravity: Vec3, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter, jolt: JoltInterface): void;
  ExtendedUpdateWithFilters(deltaTime: number, gravity: Vec3, updateSettings: ExtendedUpdateSettings, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter, jolt: JoltInterface): void;
  SetShape(shape: Shape | null, maxPenetrationDepth: number, objectLayer: number, jolt: JoltInterface): boolean;
  GetHitReductionCosMaxAngle(): number;
  SetHitReductionCosMaxAngle(cosMaxAngle: number): void;
  WalkStairs(deltaTime: number, stepUp: Vec3, stepForward: Vec3, stepForwardTest: Vec3, stepDownExtra: Vec3, broadPhaseFilter: BroadPhaseLayerFilter, objectLayerFilter: ObjectLayerFilter, bodyFilter: BodyFilter, shapeFilter: ShapeFilter, jolt: JoltInterface): boolean;
}

export interface CharacterSettings extends CharacterBaseSettings {
  mLayer: number;
  mMass: number;
  mFriction: number;
  mGravityFactor: number;
}

export interface Character extends CharacterBase {
  GetTransformedShape(): TransformedShape;
  AddToPhysicsSystem(activationMode: EActivation): void;
  RemoveFromPhysicsSystem(): void;
  Activate(): void;
  SetLayer(layer: number): void;
  GetBodyID(): number;
  GetLayer(): number;
  GetLinearVelocity(out: Vec3): Vec3;
  GetPosition(out: Vec3): Vec3;
  GetRotation(out: Quat): Quat;
  GetCenterOfMassPosition(out: Vec3): Vec3;
  GetWorldTransform(out: Mat44): Mat44;
  SetLinearVelocity(velocity: Vec3): void;
  AddLinearVelocity(velocity: Vec3): void;
  AddImpulse(impulse: Vec3): void;
  SetPosition(position: Vec3): void;
  SetLinearAndAngularVelocity(linearVelocity: Vec3, angularVelocity: Vec3): void;
  SetRotation(rotation: Quat): void;
  PostSimulation(maxSeparationDistance: number): void;
  SetShape(shape: Shape | null, maxPenetrationDepth: number): boolean;
  CheckCollision(position: Vec3, rotation: Quat, movementDirection: Vec3, maxSeparationDistance: number, shape: Shape | null, baseOffset: Vec3, collector: CollideShapeCollector): void;
}

export interface Skeleton extends ClassHandle {
  CalculateParentJointIndices(): void;
  AreJointsCorrectlyOrdered(): boolean;
  GetJointCount(): number;
  GetJointParentIndex(index: number): number;
  AddJoint(name: EmbindString, parentIndex: number): number;
  AddRootJoint(name: EmbindString): number;
  GetJointIndex(name: EmbindString): number;
  GetJointName(index: number): string;
}

export interface JointState extends ClassHandle {
  mTranslation: Vec3;
  mRotation: Quat;
}

export interface SkeletalAnimationKeyframe extends JointState {
  mTime: number;
}

export interface AnimatedJoint extends ClassHandle {
  get mJointName(): string;
  set mJointName(value: EmbindString);
  GetKeyframeCount(): number;
  ResizeKeyframes(count: number): void;
  GetKeyframe(index: number): SkeletalAnimationKeyframe | null;
}

export interface SkeletalAnimation extends ClassHandle {
  SetIsLooping(looping: boolean): void;
  IsLooping(): boolean;
  GetAnimatedJointCount(): number;
  ResizeAnimatedJoints(count: number): void;
  GetAnimatedJoint(index: number): AnimatedJoint | null;
  Sample(time: number, pose: SkeletonPose): void;
  GetDuration(): number;
}

export interface SkeletonPose extends ClassHandle {
  GetSkeleton(): Skeleton | null;
  SetSkeleton(skeleton: Skeleton | null): void;
  CalculateJointMatrices(): void;
  CalculateJointStates(): void;
  GetJoint(index: number): JointState | null;
  GetJointCount(): number;
  GetJointMatricesCount(): number;
  GetRootOffset(out: Vec3): Vec3;
  GetJointMatrix(out: Mat44, index: number): Mat44;
  SetRootOffset(offset: Vec3): void;
  SetJointMatrix(index: number, matrix: Mat44): void;
}

export interface RagdollAdditionalConstraint extends ClassHandle {
  GetConstraint(): TwoBodyConstraintSettings | null;
  SetConstraint(constraintSettings: TwoBodyConstraintSettings | null): void;
  GetBodyIdx(index: number): number;
  SetBodyIdx(index: number, value: number): void;
}

export interface RagdollPart extends BodyCreationSettings {
  SetShape(shape: Shape | null): void;
  SetToParent(constraintSettings: TwoBodyConstraintSettings | null): void;
}

export interface RagdollSettings extends ClassHandle {
  GetSkeleton(): Skeleton | null;
  SetSkeleton(skeleton: Skeleton | null): void;
  DisableParentChildCollisions(): void;
  CalculateBodyIndexToConstraintIndex(): void;
  CalculateConstraintIndexToBodyIdxPair(): void;
  CalculateConstraintPriorities(): void;
  Stabilize(): boolean;
  ResizeParts(count: number): void;
  GetPart(index: number): RagdollPart | null;
  GetConstraintIndexForBodyIndex(bodyIndex: number): number;
  AddAdditionalConstraint(bodyIdx1: number, bodyIdx2: number, constraintSettings: TwoBodyConstraintSettings | null): void;
  GetAdditionalConstraintCount(): number;
  ResizeAdditionalConstraints(count: number): void;
  GetAdditionalConstraint(index: number): RagdollAdditionalConstraint | null;
  CreateRagdoll(collisionGroup: number, userData: bigint, physicsSystem: PhysicsSystem | null): Ragdoll | null;
}

export interface Ragdoll extends ClassHandle {
  GetRagdollSettings(): RagdollSettings | null;
  AddToPhysicsSystem(activationMode: EActivation): void;
  RemoveFromPhysicsSystem(): void;
  Activate(): void;
  SetPose(pose: SkeletonPose): void;
  GetPose(pose: SkeletonPose): void;
  DriveToPoseUsingMotors(pose: SkeletonPose): void;
  ResetWarmStart(): void;
  IsActive(): boolean;
  GetConstraint(index: number): TwoBodyConstraint | null;
  SetGroupID(groupID: number): void;
  GetBodyCount(): number;
  GetBodyID(index: number): number;
  GetConstraintCount(): number;
  GetRootPosition(out: Vec3): Vec3;
  GetRootRotation(out: Quat): Quat;
  GetWorldSpaceBounds(out: AABox): AABox;
  SetLinearAndAngularVelocity(linearVelocity: Vec3, angularVelocity: Vec3): void;
  AddImpulse(impulse: Vec3): void;
  SetLinearVelocity(linearVelocity: Vec3): void;
  AddLinearVelocity(linearVelocity: Vec3): void;
  DriveToPoseUsingKinematics(pose: SkeletonPose, deltaTime: number): void;
  GetBodyIDs(): number[];
}

export interface ETireFrictionDirectionValue<T extends number> {
  value: T;
}
export type ETireFrictionDirection = ETireFrictionDirectionValue<0>|ETireFrictionDirectionValue<1>;

export interface PhysicsStepListenerContext extends ClassHandle {
  mIsFirstStep: boolean;
  mIsLastStep: boolean;
  mDeltaTime: number;
  GetPhysicsSystem(): PhysicsSystem | null;
}

export interface TireMaxImpulseCallbackResult extends ClassHandle {
  mLongitudinalImpulse: number;
  mLateralImpulse: number;
}

export interface VehicleConstraintCallbacksEm extends ClassHandle {
  SetVehicleConstraint(constraint: VehicleConstraint): void;
}

export interface VehicleConstraintCallbacksJS extends VehicleConstraintCallbacksEm {
  notifyOnDestruction(): void;
}

export interface WheeledVehicleControllerCallbacksEm extends ClassHandle {
  SetWheeledVehicleController(controller: WheeledVehicleController): void;
}

export interface WheeledVehicleControllerCallbacksJS extends WheeledVehicleControllerCallbacksEm {
  notifyOnDestruction(): void;
}

export interface VehicleEngineSettings extends ClassHandle {
  mMaxTorque: number;
  mMinRPM: number;
  mMaxRPM: number;
  mInertia: number;
  mAngularDamping: number;
  GetNormalizedTorque(): LinearCurve | null;
}

export interface VehicleTransmissionSettings extends ClassHandle {
  mMode: ETransmissionMode;
  mSwitchTime: number;
  mClutchReleaseTime: number;
  mShiftUpRPM: number;
  mShiftDownRPM: number;
  mClutchStrength: number;
  mSwitchLatency: number;
  ClearGearRatios(): void;
  ClearReverseGearRatios(): void;
  AddGearRatio(ratio: number): void;
  AddReverseGearRatio(ratio: number): void;
}

export interface VehicleDifferentialSettings extends ClassHandle {
  mLeftWheel: number;
  mRightWheel: number;
  mDifferentialRatio: number;
  mLeftRightSplit: number;
  mLimitedSlipRatio: number;
  mEngineTorqueRatio: number;
}

export interface VehicleAntiRollBar extends ClassHandle {
  mLeftWheel: number;
  mRightWheel: number;
  mStiffness: number;
}

export interface VehicleTrackSettings extends ClassHandle {
  mDrivenWheel: number;
  mDifferentialRatio: number;
  mInertia: number;
  mAngularDamping: number;
  mMaxBrakeTorque: number;
  ClearWheels(): void;
  AddWheel(wheelIndex: number): void;
}

export interface VehicleEngine extends VehicleEngineSettings {
  ClampRPM(): void;
  GetCurrentRPM(): number;
  SetCurrentRPM(rpm: number): void;
  GetAngularVelocity(): number;
  GetTorque(acceleration: number): number;
}

export interface VehicleTransmission extends VehicleTransmissionSettings {
  IsSwitchingGear(): boolean;
  GetCurrentGear(): number;
  Set(currentGear: number, clutchFriction: number): void;
  GetClutchFriction(): number;
  GetCurrentRatio(): number;
}

export interface VehicleTrack extends VehicleTrackSettings {
  mAngularVelocity: number;
}

export interface WheelSettings extends ClassHandle {
  mEnableSuspensionForcePoint: boolean;
  mPosition: Vec3;
  mSuspensionDirection: Vec3;
  mSteeringAxis: Vec3;
  mWheelUp: Vec3;
  mWheelForward: Vec3;
  mSuspensionForcePoint: Vec3;
  mSuspensionMinLength: number;
  mSuspensionMaxLength: number;
  mRadius: number;
  mWidth: number;
  mSuspensionPreloadLength: number;
  GetSuspensionSpring(): SpringSettings | null;
}

export interface WheelSettingsWV extends WheelSettings {
  mInertia: number;
  mAngularDamping: number;
  mMaxSteerAngle: number;
  mMaxBrakeTorque: number;
  mMaxHandBrakeTorque: number;
  GetLongitudinalFriction(): LinearCurve | null;
  GetLateralFriction(): LinearCurve | null;
}

export interface WheelSettingsTV extends WheelSettings {
  mLongitudinalFriction: number;
  mLateralFriction: number;
}

export interface Wheel extends ClassHandle {
  GetSettings(): WheelSettings | null;
  HasContact(): boolean;
  HasHitHardPoint(): boolean;
  GetContactBodyID(): number;
  GetContactSubShapeID(): number;
  GetContactPosition(out: Vec3): Vec3;
  GetContactPointVelocity(out: Vec3): Vec3;
  GetContactNormal(out: Vec3): Vec3;
  GetContactLongitudinal(out: Vec3): Vec3;
  GetContactLateral(out: Vec3): Vec3;
  GetRotationAngle(): number;
  GetSteerAngle(): number;
  SetSteerAngle(angle: number): void;
  SetRotationAngle(angle: number): void;
  GetAngularVelocity(): number;
  SetAngularVelocity(vel: number): void;
  GetSuspensionLength(): number;
  GetSuspensionLambda(): number;
  GetLongitudinalLambda(): number;
  GetLateralLambda(): number;
}

export interface WheelWV extends Wheel {
  mLongitudinalSlip: number;
  mLateralSlip: number;
  mCombinedLongitudinalFriction: number;
  mCombinedLateralFriction: number;
  mBrakeImpulse: number;
  GetSettings(): WheelSettingsWV | null;
}

export interface WheelTV extends Wheel {
  mTrackIndex: number;
  mCombinedLongitudinalFriction: number;
  mCombinedLateralFriction: number;
  mBrakeImpulse: number;
  GetSettings(): WheelSettingsTV | null;
}

export interface VehicleControllerSettings extends ClassHandle {
}

export interface WheeledVehicleControllerSettings extends VehicleControllerSettings {
  mDifferentialLimitedSlipRatio: number;
  GetEngine(): VehicleEngineSettings | null;
  GetTransmission(): VehicleTransmissionSettings | null;
  AddDifferential(differential: VehicleDifferentialSettings): void;
  ClearDifferentials(): void;
}

export interface MotorcycleControllerSettings extends WheeledVehicleControllerSettings {
  mMaxLeanAngle: number;
  mLeanSpringConstant: number;
  mLeanSpringDamping: number;
  mLeanSpringIntegrationCoefficient: number;
  mLeanSpringIntegrationCoefficientDecay: number;
  mLeanSmoothingFactor: number;
}

export interface TrackedVehicleControllerSettings extends VehicleControllerSettings {
  GetEngine(): VehicleEngineSettings | null;
  GetTransmission(): VehicleTransmissionSettings | null;
  GetTrack(index: number): VehicleTrackSettings | null;
}

export interface VehicleController extends ClassHandle {
  GetConstraint(): VehicleConstraint | null;
}

export interface WheeledVehicleController extends VehicleController {
  GetEngine(): VehicleEngine | null;
  GetTransmission(): VehicleTransmission | null;
  SetDriverInput(forward: number, right: number, brake: number, handBrake: number): void;
  SetForwardInput(forward: number): void;
  GetForwardInput(): number;
  SetRightInput(right: number): void;
  GetRightInput(): number;
  SetBrakeInput(brake: number): void;
  GetBrakeInput(): number;
  SetHandBrakeInput(handBrake: number): void;
  GetHandBrakeInput(): number;
  GetDifferentialLimitedSlipRatio(): number;
  SetDifferentialLimitedSlipRatio(v: number): void;
  GetWheelSpeedAtClutch(): number;
}

export interface MotorcycleController extends WheeledVehicleController {
  EnableLeanController(enable: boolean): void;
  IsLeanControllerEnabled(): boolean;
  EnableLeanSteeringLimit(enable: boolean): void;
  IsLeanSteeringLimitEnabled(): boolean;
  GetWheelBase(): number;
  SetLeanSpringConstant(c: number): void;
  GetLeanSpringConstant(): number;
  SetLeanSpringDamping(d: number): void;
  GetLeanSpringDamping(): number;
  SetLeanSpringIntegrationCoefficient(c: number): void;
  GetLeanSpringIntegrationCoefficient(): number;
  SetLeanSpringIntegrationCoefficientDecay(d: number): void;
  GetLeanSpringIntegrationCoefficientDecay(): number;
  SetLeanSmoothingFactor(f: number): void;
  GetLeanSmoothingFactor(): number;
}

export interface TrackedVehicleController extends VehicleController {
  GetEngine(): VehicleEngine | null;
  GetTransmission(): VehicleTransmission | null;
  GetTrack(index: number): VehicleTrack | null;
  SetDriverInput(forward: number, leftRatio: number, rightRatio: number, brake: number): void;
  SetForwardInput(forward: number): void;
  GetForwardInput(): number;
  SetLeftRatio(leftRatio: number): void;
  GetLeftRatio(): number;
  SetRightRatio(rightRatio: number): void;
  GetRightRatio(): number;
  SetBrakeInput(brake: number): void;
  GetBrakeInput(): number;
}

export interface VehicleCollisionTester extends ClassHandle {
}

export interface VehicleCollisionTesterRay extends VehicleCollisionTester {
}

export interface VehicleCollisionTesterCastSphere extends VehicleCollisionTester {
}

export interface VehicleCollisionTesterCastCylinder extends VehicleCollisionTester {
}

export interface VehicleConstraintSettings extends ConstraintSettings {
  mUp: Vec3;
  mForward: Vec3;
  mMaxPitchRollAngle: number;
  AddWheel(wheelSettings: WheelSettings | null): void;
  ClearWheels(): void;
  AddAntiRollBar(antiRollBar: VehicleAntiRollBar): void;
  SetController(controllerSettings: VehicleControllerSettings | null): void;
}

export interface VehicleConstraint extends Constraint {
  GetController(): VehicleController | null;
  GetWheeledController(): WheeledVehicleController | null;
  GetTrackedController(): TrackedVehicleController | null;
  GetMotorcycleController(): MotorcycleController | null;
  GetVehicleCollisionTester(): VehicleCollisionTester | null;
  GetVehicleBody(): Body | null;
  SetVehicleCollisionTester(tester: VehicleCollisionTester | null): void;
  ResetGravityOverride(): void;
  IsGravityOverridden(): boolean;
  GetWheel(index: number): Wheel | null;
  GetNumWheels(): number;
  GetNumAntiRollBars(): number;
  GetAntiRollBar(index: number): VehicleAntiRollBar | null;
  SetNumStepsBetweenCollisionTestActive(steps: number): void;
  GetNumStepsBetweenCollisionTestActive(): number;
  SetNumStepsBetweenCollisionTestInactive(steps: number): void;
  GetNumStepsBetweenCollisionTestInactive(): number;
  GetGravityOverride(out: Vec3): Vec3;
  GetLocalForward(out: Vec3): Vec3;
  GetLocalUp(out: Vec3): Vec3;
  GetWorldUp(out: Vec3): Vec3;
  GetWheelLocalTransform(out: Mat44, wheelIndex: number, wheelRight: Vec3, wheelUp: Vec3): Mat44;
  GetWheelWorldTransform(out: Mat44, wheelIndex: number, wheelRight: Vec3, wheelUp: Vec3): Mat44;
  OverrideGravity(gravity: Vec3): void;
  SetMaxPitchRollAngle(maxPitchRollAngle: number): void;
  GetMaxPitchRollAngle(): number;
}

export interface ECullModeValue<T extends number> {
  value: T;
}
export type ECullMode = ECullModeValue<0>|ECullModeValue<1>|ECullModeValue<2>;

export interface ECastShadowValue<T extends number> {
  value: T;
}
export type ECastShadow = ECastShadowValue<0>|ECastShadowValue<1>;

export interface EDrawModeValue<T extends number> {
  value: T;
}
export type EDrawMode = EDrawModeValue<0>|EDrawModeValue<1>;

export interface EShapeColorValue<T extends number> {
  value: T;
}
export type EShapeColor = EShapeColorValue<0>|EShapeColorValue<1>|EShapeColorValue<2>|EShapeColorValue<3>|EShapeColorValue<4>|EShapeColorValue<5>;

export interface ESoftBodyConstraintColorValue<T extends number> {
  value: T;
}
export type ESoftBodyConstraintColor = ESoftBodyConstraintColorValue<0>|ESoftBodyConstraintColorValue<1>|ESoftBodyConstraintColorValue<2>;

export interface BodyManagerDrawSettings extends ClassHandle {
  mDrawShapeColor: EShapeColor;
  mDrawSoftBodyConstraintColor: ESoftBodyConstraintColor;
  mDrawGetSupportFunction: boolean;
  mDrawSupportDirection: boolean;
  mDrawGetSupportingFace: boolean;
  mDrawShape: boolean;
  mDrawShapeWireframe: boolean;
  mDrawBoundingBox: boolean;
  mDrawCenterOfMassTransform: boolean;
  mDrawWorldTransform: boolean;
  mDrawVelocity: boolean;
  mDrawMassAndInertia: boolean;
  mDrawSleepStats: boolean;
  mDrawSoftBodyVertices: boolean;
  mDrawSoftBodyVertexVelocities: boolean;
  mDrawSoftBodyEdgeConstraints: boolean;
  mDrawSoftBodyBendConstraints: boolean;
  mDrawSoftBodyVolumeConstraints: boolean;
  mDrawSoftBodySkinConstraints: boolean;
  mDrawSoftBodyLRAConstraints: boolean;
  mDrawSoftBodyRods: boolean;
  mDrawSoftBodyRodStates: boolean;
  mDrawSoftBodyRodBendTwistConstraints: boolean;
  mDrawSoftBodyPredictedBounds: boolean;
}

export interface DebugRendererVertexTraits extends ClassHandle {
}

export interface DebugRendererTriangleTraits extends ClassHandle {
}

export interface DebugRendererEm extends ClassHandle {
  Initialize(): void;
  DrawBodies(system: PhysicsSystem | null, settings: BodyManagerDrawSettings | null): void;
  DrawBodies(system: PhysicsSystem | null): void;
  DrawConstraints(system: PhysicsSystem | null): void;
  DrawConstraintLimits(system: PhysicsSystem | null): void;
  DrawConstraintReferenceFrame(system: PhysicsSystem | null): void;
  DrawConstraint(constraint: Constraint | null): void;
  DrawBody(body: Body | null, color: Color, wireframe: boolean): void;
  DrawShape(shape: Shape | null, modelMatrix: Mat44, scale: Vec3, color: Color, wireframe: boolean): void;
}

export interface DebugRendererWrapper extends DebugRendererEm {
  notifyOnDestruction(): void;
}

export type Color = [ r: number, g: number, b: number, a: number ];

export type Vec3 = [ x: number, y: number, z: number ];

export type Vector2 = [ number, number ];

export type Quat = [ x: number, y: number, z: number, w: number ];

export type Vec4 = [ x: number, y: number, z: number, w: number ];

export type Float3 = [ x: number, y: number, z: number ];

export type Float2 = [ x: number, y: number ];

export type AABox = [ minX: number, minY: number, minZ: number, maxX: number, maxY: number, maxZ: number ];

export type Mat44 = [ number, number, number, number, number, number, number, number, number, number, number, number, number, number, number, number ];

interface EmbindModule {
  EBodyType: {RigidBody: EBodyTypeValue<0>, SoftBody: EBodyTypeValue<1>};
  EMotionType: {Static: EMotionTypeValue<0>, Kinematic: EMotionTypeValue<1>, Dynamic: EMotionTypeValue<2>};
  EMotionQuality: {Discrete: EMotionQualityValue<0>, LinearCast: EMotionQualityValue<1>};
  EActivation: {Activate: EActivationValue<0>, DontActivate: EActivationValue<1>};
  EShapeType: {Convex: EShapeTypeValue<0>, Compound: EShapeTypeValue<1>, Decorated: EShapeTypeValue<2>, Mesh: EShapeTypeValue<3>, HeightField: EShapeTypeValue<4>, Plane: EShapeTypeValue<10>, Empty: EShapeTypeValue<11>};
  EShapeSubType: {Sphere: EShapeSubTypeValue<0>, Box: EShapeSubTypeValue<1>, Capsule: EShapeSubTypeValue<3>, TaperedCapsule: EShapeSubTypeValue<4>, Cylinder: EShapeSubTypeValue<5>, TaperedCylinder: EShapeSubTypeValue<32>, ConvexHull: EShapeSubTypeValue<6>, StaticCompound: EShapeSubTypeValue<7>, MutableCompound: EShapeSubTypeValue<8>, RotatedTranslated: EShapeSubTypeValue<9>, Scaled: EShapeSubTypeValue<10>, OffsetCenterOfMass: EShapeSubTypeValue<11>, Mesh: EShapeSubTypeValue<12>, HeightField: EShapeSubTypeValue<13>, Plane: EShapeSubTypeValue<31>, Empty: EShapeSubTypeValue<33>};
  EConstraintSpace: {LocalToBodyCOM: EConstraintSpaceValue<0>, WorldSpace: EConstraintSpaceValue<1>};
  ESpringMode: {FrequencyAndDamping: ESpringModeValue<0>, StiffnessAndDamping: ESpringModeValue<1>};
  EOverrideMassProperties: {CalculateMassAndInertia: EOverrideMassPropertiesValue<0>, CalculateInertia: EOverrideMassPropertiesValue<1>, MassAndInertiaProvided: EOverrideMassPropertiesValue<2>};
  EAllowedDOFs: {TranslationX: EAllowedDOFsValue<1>, TranslationY: EAllowedDOFsValue<2>, TranslationZ: EAllowedDOFsValue<4>, RotationX: EAllowedDOFsValue<8>, RotationY: EAllowedDOFsValue<16>, RotationZ: EAllowedDOFsValue<32>, Plane2D: EAllowedDOFsValue<35>, All: EAllowedDOFsValue<63>};
  EStateRecorderState: {None: EStateRecorderStateValue<0>, Global: EStateRecorderStateValue<1>, Bodies: EStateRecorderStateValue<2>, Contacts: EStateRecorderStateValue<4>, Constraints: EStateRecorderStateValue<8>, All: EStateRecorderStateValue<15>};
  EBackFaceMode: {IgnoreBackFaces: EBackFaceModeValue<0>, CollideWithBackFaces: EBackFaceModeValue<1>};
  EGroundState: {OnGround: EGroundStateValue<0>, OnSteepGround: EGroundStateValue<1>, NotSupported: EGroundStateValue<2>, InAir: EGroundStateValue<3>};
  ValidateResult: {AcceptAllContactsForThisBodyPair: ValidateResultValue<0>, AcceptContact: ValidateResultValue<1>, RejectContact: ValidateResultValue<2>, RejectAllContactsForThisBodyPair: ValidateResultValue<3>};
  SoftBodyValidateResult: {AcceptContact: SoftBodyValidateResultValue<0>, RejectContact: SoftBodyValidateResultValue<1>};
  EActiveEdgeMode: {CollideOnlyWithActive: EActiveEdgeModeValue<0>, CollideWithAll: EActiveEdgeModeValue<1>};
  ECollectFacesMode: {CollectFaces: ECollectFacesModeValue<0>, NoFaces: ECollectFacesModeValue<1>};
  SixDOFConstraintSettings_EAxis: {TranslationX: SixDOFConstraintSettings_EAxisValue<0>, TranslationY: SixDOFConstraintSettings_EAxisValue<1>, TranslationZ: SixDOFConstraintSettings_EAxisValue<2>, RotationX: SixDOFConstraintSettings_EAxisValue<3>, RotationY: SixDOFConstraintSettings_EAxisValue<4>, RotationZ: SixDOFConstraintSettings_EAxisValue<5>};
  EConstraintType: {Constraint: EConstraintTypeValue<0>, TwoBodyConstraint: EConstraintTypeValue<1>};
  EConstraintSubType: {Fixed: EConstraintSubTypeValue<0>, Point: EConstraintSubTypeValue<1>, Hinge: EConstraintSubTypeValue<2>, Slider: EConstraintSubTypeValue<3>, Distance: EConstraintSubTypeValue<4>, Cone: EConstraintSubTypeValue<5>, SwingTwist: EConstraintSubTypeValue<6>, SixDOF: EConstraintSubTypeValue<7>, Path: EConstraintSubTypeValue<8>, Vehicle: EConstraintSubTypeValue<9>, RackAndPinion: EConstraintSubTypeValue<10>, Gear: EConstraintSubTypeValue<11>, Pulley: EConstraintSubTypeValue<12>};
  EMotorState: {Off: EMotorStateValue<0>, Velocity: EMotorStateValue<1>, Position: EMotorStateValue<2>};
  ETransmissionMode: {Auto: ETransmissionModeValue<0>, Manual: ETransmissionModeValue<1>};
  ESwingType: {Cone: ESwingTypeValue<0>, Pyramid: ESwingTypeValue<1>};
  EPathRotationConstraintType: {Free: EPathRotationConstraintTypeValue<0>, ConstrainAroundTangent: EPathRotationConstraintTypeValue<1>, ConstrainAroundNormal: EPathRotationConstraintTypeValue<2>, ConstrainAroundBinormal: EPathRotationConstraintTypeValue<3>, ConstrainToPath: EPathRotationConstraintTypeValue<4>, FullyConstrained: EPathRotationConstraintTypeValue<5>};
  EBendType: {None: EBendTypeValue<0>, Distance: EBendTypeValue<1>, Dihedral: EBendTypeValue<2>};
  ELRAType: {None: ELRATypeValue<0>, EuclideanDistance: ELRATypeValue<1>, GeodesicDistance: ELRATypeValue<2>};
  MeshShapeSettings_EBuildQuality: {FavorRuntimePerformance: MeshShapeSettings_EBuildQualityValue<0>, FavorBuildSpeed: MeshShapeSettings_EBuildQualityValue<1>};
  Shape: {};
  ConvexShape: {};
  BoxShape: {
    new(halfExtent: Vec3): BoxShape;
    new(halfExtent: Vec3, convexRadius: number): BoxShape;
  };
  SphereShape: {
    new(radius: number): SphereShape;
  };
  ShapeResult: {};
  ShapeSettings: {};
  ConvexShapeSettings: {};
  BoxShapeSettings: {
    new(halfExtent: Vec3): BoxShapeSettings;
    new(halfExtent: Vec3, convexRadius: number): BoxShapeSettings;
  };
  SphereShapeSettings: {
    new(radius: number): SphereShapeSettings;
  };
  CapsuleShape: {
    new(halfHeightOfCylinder: number, radius: number): CapsuleShape;
  };
  CapsuleShapeSettings: {
    new(halfHeightOfCylinder: number, radius: number): CapsuleShapeSettings;
  };
  TaperedCapsuleShape: {};
  TaperedCapsuleShapeSettings: {
    new(halfHeightOfTaperedCylinder: number, topRadius: number, bottomRadius: number): TaperedCapsuleShapeSettings;
  };
  CylinderShape: {
    new(halfHeight: number, radius: number): CylinderShape;
  };
  CylinderShapeSettings: {
    new(halfHeight: number, radius: number): CylinderShapeSettings;
  };
  TaperedCylinderShape: {};
  TaperedCylinderShapeSettings: {
    new(halfHeight: number, topRadius: number, bottomRadius: number): TaperedCylinderShapeSettings;
  };
  ConvexHullShape: {};
  ConvexHullShapeSettings: {
    new(points: ArrayVec3): ConvexHullShapeSettings;
  };
  CompoundShapeSubShape: {};
  CompoundShape: {};
  CompoundShapeSettings: {};
  StaticCompoundShape: {};
  StaticCompoundShapeSettings: {
    new(): StaticCompoundShapeSettings;
  };
  MutableCompoundShape: {};
  MutableCompoundShapeSettings: {
    new(): MutableCompoundShapeSettings;
  };
  DecoratedShape: {};
  DecoratedShapeSettings: {};
  RotatedTranslatedShape: {
    new(position: Vec3, rotation: Quat, shape: Shape | null): RotatedTranslatedShape;
  };
  RotatedTranslatedShapeSettings: {
    new(position: Vec3, rotation: Quat, shape: Shape | null): RotatedTranslatedShapeSettings;
  };
  ScaledShape: {
    new(shape: Shape | null, scale: Vec3): ScaledShape;
  };
  ScaledShapeSettings: {
    new(shape: Shape | null, scale: Vec3): ScaledShapeSettings;
  };
  OffsetCenterOfMassShape: {
    new(shape: Shape | null, offset: Vec3): OffsetCenterOfMassShape;
  };
  OffsetCenterOfMassShapeSettings: {
    new(offset: Vec3, shape: Shape | null): OffsetCenterOfMassShapeSettings;
  };
  EmptyShape: {
    new(centerOfMass: any): EmptyShape;
    new(centerOfMass: Vec3): EmptyShape;
  };
  EmptyShapeSettings: {
    new(centerOfMass: any): EmptyShapeSettings;
    new(centerOfMass: Vec3): EmptyShapeSettings;
  };
  Plane: {
    new(normal: any): Plane;
    new(normal: Vec3, constant: number): Plane;
    sFromPointAndNormal(point: Vec3, normal: Vec3): Plane;
    sFromPointsCCW(v1: Vec3, v2: Vec3, v3: Vec3): Plane;
  };
  MeshShape: {};
  MeshShapeSettings: {
    new(vertices: Float32Array | number[], indices: Uint32Array | number[]): MeshShapeSettings;
  };
  HeightFieldShape: {};
  HeightFieldShapeSettings: {
    new(samples: Float32Array | number[], offset: Vec3, scale: Vec3, sampleCount: number): HeightFieldShapeSettings;
  };
  PlaneShape: {
    new(plane: Plane): PlaneShape;
  };
  PlaneShapeSettings: {
    new(plane: Plane): PlaneShapeSettings;
  };
  TransformedShape: {};
  SoftBodySharedSettings: {
    new(): SoftBodySharedSettings;
  };
  SoftBodyCreationSettings: {
    new(settings: any): SoftBodyCreationSettings;
    new(settings: SoftBodySharedSettings | null, position: Vec3, rotation: Quat, objectLayer: number): SoftBodyCreationSettings;
  };
  SoftBodyVertex: {
    new(): SoftBodyVertex;
  };
  SoftBodyContactSettings: {
    new(): SoftBodyContactSettings;
  };
  SoftBodyManifold: {};
  SoftBodyShape: {};
  SoftBodySharedSettingsVertex: {
    new(): SoftBodySharedSettingsVertex;
  };
  SoftBodySharedSettingsFace: {
    new(vertex0: any): SoftBodySharedSettingsFace;
    new(vertex0: number, vertex1: number, vertex2: number, materialIndex: number): SoftBodySharedSettingsFace;
  };
  SoftBodySharedSettingsEdge: {
    new(vertex0: any): SoftBodySharedSettingsEdge;
    new(vertex0: number, vertex1: number, compliance: number): SoftBodySharedSettingsEdge;
  };
  SoftBodySharedSettingsDihedralBend: {
    new(vertex0: any): SoftBodySharedSettingsDihedralBend;
    new(vertex0: number, vertex1: number, vertex2: number, vertex3: number, compliance: number): SoftBodySharedSettingsDihedralBend;
  };
  SoftBodySharedSettingsVolume: {
    new(vertex0: any): SoftBodySharedSettingsVolume;
    new(vertex0: number, vertex1: number, vertex2: number, vertex3: number, compliance: number): SoftBodySharedSettingsVolume;
  };
  SoftBodySharedSettingsInvBind: {
    new(jointIndex: any): SoftBodySharedSettingsInvBind;
    new(jointIndex: number, invBind: Mat44): SoftBodySharedSettingsInvBind;
  };
  SoftBodySharedSettingsSkinWeight: {
    new(invBindIndex: any): SoftBodySharedSettingsSkinWeight;
    new(invBindIndex: number, weight: number): SoftBodySharedSettingsSkinWeight;
  };
  SoftBodySharedSettingsSkinned: {
    new(vertex: any): SoftBodySharedSettingsSkinned;
    new(vertex: number, maxDistance: number, backStopDistance: number, backStopRadius: number): SoftBodySharedSettingsSkinned;
  };
  SoftBodySharedSettingsLRA: {
    new(vertex0: any): SoftBodySharedSettingsLRA;
    new(vertex0: number, vertex1: number, maxDistance: number): SoftBodySharedSettingsLRA;
  };
  SoftBodySharedSettingsRodStretchShear: {
    new(vertex0: any): SoftBodySharedSettingsRodStretchShear;
    new(vertex0: number, vertex1: number, compliance: number): SoftBodySharedSettingsRodStretchShear;
  };
  SoftBodySharedSettingsRodBendTwist: {
    new(rod0: any): SoftBodySharedSettingsRodBendTwist;
    new(rod0: number, rod1: number, compliance: number): SoftBodySharedSettingsRodBendTwist;
  };
  SoftBodySharedSettingsVertexAttributes: {
    new(): SoftBodySharedSettingsVertexAttributes;
  };
  SoftBodyContactListener: {
    implement(obj: any): SoftBodyContactListenerWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  SoftBodyContactListenerWrapper: {};
  BodyCreationSettings: {
    new(shape: any): BodyCreationSettings;
    new(shape: Shape | null, position: Vec3, rotation: Quat, motionType: EMotionType, objectLayer: number): BodyCreationSettings;
  };
  BodyInterface: {};
  Body: {};
  MotionProperties: {};
  SoftBodyMotionProperties: {};
  ContactManifold: {};
  ContactSettings: {
    new(): ContactSettings;
  };
  ContactListener: {
    implement(obj: any): ContactListenerWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  ContactListenerWrapper: {};
  ThreadedContactListener: {
    new(): ThreadedContactListener;
  };
  BodyActivationListener: {
    implement(obj: any): BodyActivationListenerWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  BodyActivationListenerWrapper: {};
  ContactListenerBuffer: {
    new(): ContactListenerBuffer;
  };
  ActiveBodyBuffer: {
    new(): ActiveBodyBuffer;
  };
  ArrayVec3: {
    new(): ArrayVec3;
  };
  BodyLockInterface: {};
  PhysicsSystem: {};
  PhysicsSettings: {
    new(): PhysicsSettings;
  };
  PhysicsStepListener: {
    implement(obj: any): PhysicsStepListenerWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  PhysicsStepListenerWrapper: {};
  RRayCast: {
    new(origin: Vec3, direction: Vec3): RRayCast;
  };
  RayCast: {
    new(origin: Vec3, direction: Vec3): RayCast;
  };
  OrientedBox: {
    new(orientation: any): OrientedBox;
    new(orientation: Mat44, halfExtents: Vec3): OrientedBox;
  };
  AABoxCast: {
    new(): AABoxCast;
  };
  BroadPhaseCastResult: {};
  RayCastResult: {
    new(): RayCastResult;
  };
  BroadPhaseLayerFilter: {
    new(): BroadPhaseLayerFilter;
    implement(obj: any): BroadPhaseLayerFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  BroadPhaseLayerFilterWrapper: {};
  ObjectLayerFilter: {
    new(): ObjectLayerFilter;
    implement(obj: any): ObjectLayerFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  ObjectLayerFilterWrapper: {};
  BodyFilter: {
    new(): BodyFilter;
    implement(obj: any): BodyFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  BodyFilterWrapper: {};
  SpecifiedBroadPhaseLayerFilter: {
    new(layer: BroadPhaseLayer): SpecifiedBroadPhaseLayerFilter;
  };
  SpecifiedObjectLayerFilter: {
    new(layer: number): SpecifiedObjectLayerFilter;
  };
  IgnoreSingleBodyFilter: {
    new(bodyID: number): IgnoreSingleBodyFilter;
  };
  IgnoreMultipleBodiesFilter: {
    new(): IgnoreMultipleBodiesFilter;
  };
  ShapeFilter: {
    new(): ShapeFilter;
    implement(obj: any): ShapeFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  ShapeFilterWrapper: {};
  SimShapeFilter: {
    new(): SimShapeFilter;
    implement(obj: any): SimShapeFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  SimShapeFilterWrapper: {};
  DefaultBroadPhaseLayerFilter: {
    new(objectVsBroadPhaseLayerFilter: ObjectVsBroadPhaseLayerFilter, objectLayer: number): DefaultBroadPhaseLayerFilter;
  };
  DefaultObjectLayerFilter: {
    new(objectLayerPairFilter: ObjectLayerPairFilter, objectLayer: number): DefaultObjectLayerFilter;
  };
  NarrowPhaseQuery: {};
  BroadPhaseQuery: {};
  CollideShapeResult: {};
  ShapeCastResult: {};
  CollidePointResult: {};
  RayCastSettings: {
    new(): RayCastSettings;
  };
  CollideShapeSettings: {
    new(): CollideShapeSettings;
  };
  ShapeCastSettings: {
    new(): ShapeCastSettings;
  };
  RShapeCast: {
    new(shape: Shape | null, scale: Vec3, centerOfMassStart: Mat44, direction: Vec3): RShapeCast;
    sFromWorldTransform(shape: Shape | null, scale: Vec3, worldTransform: Mat44, direction: Vec3): RShapeCast;
  };
  CastRayCollector: {
    implement(obj: any): CastRayCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CastRayCollectorWrapper: {};
  TransformedShapeCollector: {
    implement(obj: any): TransformedShapeCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  TransformedShapeCollectorWrapper: {};
  CollideShapeCollector: {
    implement(obj: any): CollideShapeCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CollideShapeCollectorWrapper: {};
  CastShapeCollector: {
    implement(obj: any): CastShapeCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CastShapeCollectorWrapper: {};
  CollidePointCollector: {
    implement(obj: any): CollidePointCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CollidePointCollectorWrapper: {};
  RayCastBodyCollector: {
    implement(obj: any): RayCastBodyCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  RayCastBodyCollectorWrapper: {};
  CastShapeBodyCollector: {
    implement(obj: any): CastShapeBodyCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CastShapeBodyCollectorWrapper: {};
  CollideShapeBodyCollector: {
    implement(obj: any): CollideShapeBodyCollectorWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CollideShapeBodyCollectorWrapper: {};
  CollideShapeClosestHitCollector: {
    new(): CollideShapeClosestHitCollector;
  };
  CollideShapeAllHitCollector: {
    new(): CollideShapeAllHitCollector;
  };
  CastShapeClosestHitCollector: {
    new(): CastShapeClosestHitCollector;
  };
  CastShapeAllHitCollector: {
    new(): CastShapeAllHitCollector;
  };
  CollidePointClosestHitCollector: {
    new(): CollidePointClosestHitCollector;
  };
  CollidePointAllHitCollector: {
    new(): CollidePointAllHitCollector;
  };
  CastRayClosestHitCollector: {
    new(): CastRayClosestHitCollector;
  };
  CastRayAllHitCollector: {
    new(): CastRayAllHitCollector;
  };
  RayCastBodyClosestHitCollector: {
    new(): RayCastBodyClosestHitCollector;
  };
  RayCastBodyAllHitCollector: {
    new(): RayCastBodyAllHitCollector;
  };
  CastShapeBodyClosestHitCollector: {
    new(): CastShapeBodyClosestHitCollector;
  };
  CastShapeBodyAllHitCollector: {
    new(): CastShapeBodyAllHitCollector;
  };
  sGetFixedToWorldBody(): Body | null;
  PhysicsMaterial: {
    new(): PhysicsMaterial;
  };
  PhysicsMaterialSimple: {
    new(name: any): PhysicsMaterialSimple;
    new(name: EmbindString, color: Color): PhysicsMaterialSimple;
  };
  GroupFilter: {
    implement(obj: any): GroupFilterJS;
    extend(name: EmbindString, obj: any): any;
  };
  GroupFilterJS: {};
  GroupFilterTable: {
    new(numSubGroups: number): GroupFilterTable;
  };
  CollisionGroup: {
    new(groupFilter: any): CollisionGroup;
    new(groupFilter: GroupFilter | null, groupID: number, subGroupID: number): CollisionGroup;
  };
  MassProperties: {
    new(): MassProperties;
  };
  SpringSettings: {
    new(): SpringSettings;
  };
  LinearCurvePoint: {
    new(): LinearCurvePoint;
  };
  LinearCurve: {
    new(): LinearCurve;
  };
  MotorSettings: {
    new(): MotorSettings;
  };
  Constraint: {};
  TwoBodyConstraint: {};
  ConstraintSettings: {};
  TwoBodyConstraintSettings: {};
  FixedConstraintSettings: {
    new(): FixedConstraintSettings;
  };
  FixedConstraint: {};
  PointConstraintSettings: {
    new(): PointConstraintSettings;
  };
  PointConstraint: {};
  DistanceConstraintSettings: {
    new(): DistanceConstraintSettings;
  };
  DistanceConstraint: {};
  HingeConstraintSettings: {
    new(): HingeConstraintSettings;
  };
  HingeConstraint: {};
  ConeConstraintSettings: {
    new(): ConeConstraintSettings;
  };
  ConeConstraint: {};
  SliderConstraintSettings: {
    new(): SliderConstraintSettings;
  };
  SliderConstraint: {};
  SwingTwistConstraintSettings: {
    new(): SwingTwistConstraintSettings;
  };
  SwingTwistConstraint: {};
  SixDOFConstraintSettings: {
    new(): SixDOFConstraintSettings;
  };
  SixDOFConstraint: {};
  GearConstraintSettings: {
    new(): GearConstraintSettings;
  };
  GearConstraint: {};
  RackAndPinionConstraintSettings: {
    new(): RackAndPinionConstraintSettings;
  };
  RackAndPinionConstraint: {};
  PulleyConstraintSettings: {
    new(): PulleyConstraintSettings;
  };
  PulleyConstraint: {};
  PathConstraintPath: {
    implement(obj: any): PathConstraintPathWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  PathConstraintPathWrapper: {};
  PathConstraintPathHermite: {
    new(): PathConstraintPathHermite;
  };
  PathConstraintSettings: {
    new(): PathConstraintSettings;
  };
  PathConstraint: {};
  BroadPhaseLayer: {
    new(value: number): BroadPhaseLayer;
  };
  BroadPhaseLayerInterface: {
    implement(obj: any): BroadPhaseLayerInterfaceWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  BroadPhaseLayerInterfaceWrapper: {};
  ObjectVsBroadPhaseLayerFilter: {
    implement(obj: any): ObjectVsBroadPhaseLayerFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  ObjectVsBroadPhaseLayerFilterWrapper: {};
  ObjectLayerPairFilter: {
    implement(obj: any): ObjectLayerPairFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  ObjectLayerPairFilterWrapper: {};
  ObjectLayerPairFilterTable: {
    new(numObjectLayers: number): ObjectLayerPairFilterTable;
  };
  BroadPhaseLayerInterfaceTable: {
    new(numObjectLayers: number, numBroadPhaseLayers: number): BroadPhaseLayerInterfaceTable;
  };
  ObjectVsBroadPhaseLayerFilterTable: {
    new(broadPhaseLayerInterface: BroadPhaseLayerInterface, numBroadPhaseLayers: number, objectLayerPairFilter: ObjectLayerPairFilter, numObjectLayers: number): ObjectVsBroadPhaseLayerFilterTable;
  };
  ObjectLayerPairFilterMask: {
    new(): ObjectLayerPairFilterMask;
    sGetObjectLayer(group: number, mask: number): number;
    sGetGroup(objectLayer: number): number;
    sGetMask(objectLayer: number): number;
  };
  BroadPhaseLayerInterfaceMask: {
    new(numBroadPhaseLayers: number): BroadPhaseLayerInterfaceMask;
  };
  ObjectVsBroadPhaseLayerFilterMask: {
    new(broadPhaseLayerInterface: BroadPhaseLayerInterfaceMask): ObjectVsBroadPhaseLayerFilterMask;
  };
  StateRecorder: {};
  StateRecorderFilter: {
    implement(obj: any): StateRecorderFilterWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  StateRecorderFilterWrapper: {};
  StateRecorderImpl: {
    new(): StateRecorderImpl;
  };
  JoltSettings: {
    new(): JoltSettings;
  };
  JoltInterface: {
    new(settings: JoltSettings): JoltInterface;
    sGetFreeMemory(): number;
  };
  CharacterContactSettings: {};
  CharacterContactListener: {
    implement(obj: CharacterContactListenerCallbacks): CharacterContactListenerWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CharacterContactListenerWrapper: {};
  CharacterID: {
    new(): CharacterID;
    sNextCharacterID(): CharacterID;
    sSetNextCharacterID(nextValue: number): void;
  };
  CharacterVsCharacterCollision: {
    implement(obj: any): CharacterVsCharacterCollisionWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  CharacterVsCharacterCollisionWrapper: {};
  CharacterVsCharacterCollisionSimple: {
    new(): CharacterVsCharacterCollisionSimple;
  };
  CharacterVirtualContact: {};
  ArrayCharacterVirtualContact: {
    new(): ArrayCharacterVirtualContact;
  };
  CharacterBaseSettings: {};
  CharacterVirtualSettings: {
    new(): CharacterVirtualSettings;
  };
  ExtendedUpdateSettings: {
    new(): ExtendedUpdateSettings;
  };
  CharacterBase: {};
  CharacterVirtual: {
    new(settings: CharacterVirtualSettings | null, position: Vec3, rotation: Quat, physicsSystem: PhysicsSystem | null): CharacterVirtual;
  };
  CharacterSettings: {
    new(): CharacterSettings;
  };
  Character: {
    new(settings: CharacterSettings | null, position: Vec3, rotation: Quat, userData: bigint, physicsSystem: PhysicsSystem | null): Character;
  };
  Skeleton: {
    new(): Skeleton;
  };
  JointState: {};
  SkeletalAnimationKeyframe: {};
  AnimatedJoint: {};
  SkeletalAnimation: {
    new(): SkeletalAnimation;
  };
  SkeletonPose: {
    new(): SkeletonPose;
  };
  RagdollAdditionalConstraint: {
    new(): RagdollAdditionalConstraint;
  };
  RagdollPart: {};
  RagdollSettings: {
    new(): RagdollSettings;
  };
  Ragdoll: {};
  ETireFrictionDirection: {Longitudinal: ETireFrictionDirectionValue<0>, Lateral: ETireFrictionDirectionValue<1>};
  PhysicsStepListenerContext: {};
  TireMaxImpulseCallbackResult: {};
  VehicleConstraintCallbacksEm: {
    implement(obj: any): VehicleConstraintCallbacksJS;
    extend(name: EmbindString, obj: any): any;
  };
  VehicleConstraintCallbacksJS: {};
  WheeledVehicleControllerCallbacksEm: {
    implement(obj: any): WheeledVehicleControllerCallbacksJS;
    extend(name: EmbindString, obj: any): any;
  };
  WheeledVehicleControllerCallbacksJS: {};
  VehicleEngineSettings: {};
  VehicleTransmissionSettings: {};
  VehicleDifferentialSettings: {
    new(): VehicleDifferentialSettings;
  };
  VehicleAntiRollBar: {
    new(): VehicleAntiRollBar;
  };
  VehicleTrackSettings: {};
  VehicleEngine: {};
  VehicleTransmission: {};
  VehicleTrack: {};
  WheelSettings: {};
  WheelSettingsWV: {
    new(): WheelSettingsWV;
  };
  WheelSettingsTV: {
    new(): WheelSettingsTV;
  };
  Wheel: {};
  WheelWV: {};
  WheelTV: {};
  VehicleControllerSettings: {};
  WheeledVehicleControllerSettings: {
    new(): WheeledVehicleControllerSettings;
  };
  MotorcycleControllerSettings: {
    new(): MotorcycleControllerSettings;
  };
  TrackedVehicleControllerSettings: {
    new(): TrackedVehicleControllerSettings;
  };
  VehicleController: {};
  WheeledVehicleController: {};
  MotorcycleController: {};
  TrackedVehicleController: {};
  VehicleCollisionTester: {};
  VehicleCollisionTesterRay: {
    new(objectLayer: number): VehicleCollisionTesterRay;
  };
  VehicleCollisionTesterCastSphere: {
    new(objectLayer: number, radius: number): VehicleCollisionTesterCastSphere;
  };
  VehicleCollisionTesterCastCylinder: {
    new(objectLayer: number, convexRadiusFraction: number): VehicleCollisionTesterCastCylinder;
  };
  VehicleConstraintSettings: {
    new(): VehicleConstraintSettings;
  };
  VehicleConstraint: {
    new(body: Body, settings: VehicleConstraintSettings): VehicleConstraint;
  };
  ECullMode: {CullBackFace: ECullModeValue<0>, CullFrontFace: ECullModeValue<1>, Off: ECullModeValue<2>};
  ECastShadow: {On: ECastShadowValue<0>, Off: ECastShadowValue<1>};
  EDrawMode: {Solid: EDrawModeValue<0>, Wireframe: EDrawModeValue<1>};
  EShapeColor: {InstanceColor: EShapeColorValue<0>, ShapeTypeColor: EShapeColorValue<1>, MotionTypeColor: EShapeColorValue<2>, SleepColor: EShapeColorValue<3>, IslandColor: EShapeColorValue<4>, MaterialColor: EShapeColorValue<5>};
  ESoftBodyConstraintColor: {ConstraintType: ESoftBodyConstraintColorValue<0>, ConstraintGroup: ESoftBodyConstraintColorValue<1>, ConstraintOrder: ESoftBodyConstraintColorValue<2>};
  BodyManagerDrawSettings: {
    new(): BodyManagerDrawSettings;
  };
  DebugRendererVertexTraits: {
    mPositionOffset(): number;
    mNormalOffset(): number;
    mUVOffset(): number;
    mSize(): number;
  };
  DebugRendererTriangleTraits: {
    mVOffset(): number;
    mSize(): number;
  };
  DebugRendererEm: {
    implement(obj: any): DebugRendererWrapper;
    extend(name: EmbindString, obj: any): any;
  };
  DebugRendererWrapper: {};
  addVehicleStepListener(_0: PhysicsSystem | null, _1: VehicleConstraint | null): void;
  getContactBodyID(_0: number): number;
  getContactBodyCOM(_0: number): Vec3;
  rotateVectorByBody(_0: number, _1: Vec3): Vec3;
  setContactRelativeLinearSurfaceVelocity(_0: number, _1: Vec3): void;
  setContactRelativeAngularSurfaceVelocity(_0: number, _1: Vec3): void;
  cNoCollisionValue: number;
}


/** Return from a *ContactSolve callback to override the resolved character velocity; omit (or return nothing) to keep Jolt's. */
export type CharacterVelocityOverride = { velocity?: Vec3 };
/** Return from OnAdjustBodyVelocity to override the contacting body's velocity; omit a field to keep it. */
export type AdjustedBodyVelocity = { linear?: Vec3; angular?: Vec3 };
/** Shape of the object passed to `CharacterContactListener.implement(...)`. Every callback is optional.
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

export type JoltModule = WasmModule & EmbindModule & JoltFacade;
export default function JoltFactory (options?: unknown): Promise<JoltModule>;
