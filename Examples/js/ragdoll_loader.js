import { ObjectStreamIn } from './object_stream.js';
import { LAYER_MOVING } from './example.js';

export const RagdollLoader = {
	load: async function (Jolt, filename, EMotionType, sConstraintType) {
		const text = await fetch(filename).then(res => res.text());
		const objects = ObjectStreamIn.sReadObject(text);

		const { mParts, mSkeleton } = objects.result;
		const skeleton = new Jolt.Skeleton();
		const bones = mSkeleton.mJoints.map(bone => bone.mName);
		mSkeleton.mJoints.forEach(joint => {
			skeleton.AddJoint(joint.mName, bones.indexOf(joint.mParentName));
		});

		const vec3 = [0, 0, 0];
		const quat = [0, 0, 0, 1];

		const setValues = (target, source) => {
			Object.entries(source).forEach(([name, value]) => {
				const valueType = typeof value;
				if (target[name] !== undefined && value) {
					if (valueType == 'number' || valueType == 'boolean') {
						target[name] = value;
					} else if (value._type) {
						switch (value._type) {
							case 'vec3':
							case 'float3':
							case 'double3':
							case 'dvec3':
							case 'quat':
								target[name] = [...value.value];
								break;
							case 'mat44':
							case 'dmat44':
								// mat44 properties are not commonly used in ragdoll settings
								break;
						}
					} else if (valueType == 'object' && target[name]?.$$ !== undefined) {
						setValues(target[name], value);
					}
				}
			});
		};

		function getShape(tofShape) {
			switch (tofShape._class) {
				case 'StaticCompoundShapeSettings': {
					const settings = new Jolt.StaticCompoundShapeSettings();
					setValues(settings, tofShape);
					tofShape.mSubShapes.forEach(subShape => {
						const subShapeSettings = getShape(subShape.mShape);
						const subResult = subShapeSettings.Create();
						if (subResult.HasError()) throw new Error('SubShape error: ' + subResult.GetError());
						settings.AddShape(subShape.mPosition.value, subShape.mRotation.value, subResult.Get());
						subShapeSettings.delete();
					});
					return settings;
				}
				case 'CapsuleShapeSettings': {
					const { mHalfHeightOfCylinder, mRadius } = tofShape;
					const settings = new Jolt.CapsuleShapeSettings(mHalfHeightOfCylinder, mRadius);
					setValues(settings, tofShape);
					return settings;
				}
				case 'TaperedCapsuleShapeSettings': {
					const { mHalfHeightOfTaperedCylinder, mTopRadius, mBottomRadius } = tofShape;
					const settings = new Jolt.TaperedCapsuleShapeSettings(mHalfHeightOfTaperedCylinder, mTopRadius, mBottomRadius);
					setValues(settings, tofShape);
					return settings;
				}
				case 'BoxShapeSettings': {
					const { mHalfExtent, mConvexRadius } = tofShape;
					const settings = new Jolt.BoxShapeSettings(mHalfExtent.value, mConvexRadius);
					setValues(settings, tofShape);
					return settings;
				}
			}
		}

		function overrideConstraint(original, type) {
			let settings;
			switch (type) {
				case "Fixed":
					settings = new Jolt.FixedConstraintSettings();
					settings.mPoint1 = settings.mPoint2 = original.mPosition1;
					break;
				case "Point":
					settings = new Jolt.PointConstraintSettings();
					settings.mPoint1 = settings.mPoint2 = original.mPosition1;
					break;
				case "Hinge":
					settings = new Jolt.HingeConstraintSettings();
					settings.mPoint1 = original.mPosition1;
					settings.mHingeAxis1 = original.mPlaneAxis1;
					settings.mNormalAxis1 = original.mTwistAxis1;
					settings.mPoint2 = original.mPosition2;
					settings.mHingeAxis2 = original.mPlaneAxis2;
					settings.mNormalAxis2 = original.mTwistAxis2;
					settings.mLimitsMin = -original.mNormalHalfConeAngle;
					settings.mLimitsMax = original.mNormalHalfConeAngle;
					settings.mMaxFrictionTorque = original.mMaxFrictionTorque;
					break;
				case "Slider":
					settings = new Jolt.SliderConstraintSettings();
					settings.mPoint1 = settings.mPoint2 = original.mPosition1;
					settings.mSliderAxis1 = settings.mSliderAxis2 = original.mTwistAxis1;
					settings.mNormalAxis1 = settings.mNormalAxis2 = original.mTwistAxis1;
					settings.mLimitsMin = -1.0;
					settings.mLimitsMax = 1.0;
					settings.mMaxFrictionForce = original.mMaxFrictionTorque;
					break;
				case "Cone":
					settings = new Jolt.ConeConstraintSettings();
					settings.mPoint1 = original.mPosition1;
					settings.mTwistAxis1 = original.mTwistAxis1;
					settings.mPoint2 = original.mPosition2;
					settings.mTwistAxis2 = original.mTwistAxis2;
					settings.mHalfConeAngle = original.mNormalHalfConeAngle;
					break;
				case "Ragdoll":
					settings = original;
					break;
			}
			if (settings && original !== settings) {
				original.delete();
				return settings;
			}
			return original;
		}

		const ragdoll = new Jolt.RagdollSettings();
		ragdoll.SetSkeleton(skeleton);
		ragdoll.ResizeParts(skeleton.GetJointCount());
		for (let p = 0; p < skeleton.GetJointCount(); ++p) {
			const part = ragdoll.GetPart(p);
			const mPart = mParts[p];
			setValues(part, mPart);
			const shapeSettings = getShape(mPart.mShape);
			const shapeResult = shapeSettings.Create();
			if (shapeResult.HasError()) {
				throw new Error('Shape Error: ' + shapeResult.GetError());
			}
			shapeSettings.delete();

			part.mObjectLayer = LAYER_MOVING;
			part.mMotionType = EMotionType;
			const group = part.GetCollisionGroup();
			group.SetGroupID(mPart.mCollisionGroup.mGroupID);
			group.SetSubGroupID(mPart.mCollisionGroup.mSubGroupID);
			part.SetShape(shapeResult.Get());

			const constraintType = mPart.mToParent && mPart.mToParent._class;
			if (constraintType && Jolt[constraintType]) {
				const constraintSettings = new Jolt[constraintType]();
				setValues(constraintSettings, mPart.mToParent);
				const constraint = overrideConstraint(constraintSettings, sConstraintType);
				for (const getter of ['GetMotorSettings', 'GetSwingMotorSettings', 'GetTwistMotorSettings']) {
					if (typeof constraint[getter] === 'function') {
						const ms = constraint[getter]();
						if (ms) {
							const spring = ms.GetSpringSettings();
							spring.mFrequency = 20;
							spring.mDamping = 0;
						}
					}
				}
				part.SetToParent(constraint);
			}
		}

		ragdoll.GetSkeleton().CalculateParentJointIndices();
		ragdoll.Stabilize();
		ragdoll.CalculateBodyIndexToConstraintIndex();
		ragdoll.CalculateConstraintIndexToBodyIdxPair();

		vec3; quat; // referenced to avoid lint warnings (used above by value spread)
		// Shapes, skeleton and constraint settings are all Ref<>-bound: their C++ objects are
		// kept alive by the RagdollSettings ref chain, so GC of the JS handles is harmless. The
		// caller only needs to keep the Ragdoll (and these settings) alive — see the rig examples.
		return ragdoll;
	},

	loadAnimation: async function (Jolt, filename) {
		const text = await fetch(filename).then(res => res.text());
		const objects = ObjectStreamIn.sReadObject(text);
		const { mAnimatedJoints } = objects.result;
		const anim = new Jolt.SkeletalAnimation();
		anim.ResizeAnimatedJoints(mAnimatedJoints.length);
		mAnimatedJoints.forEach((_joint, i) => {
			const joint = anim.GetAnimatedJoint(i);
			joint.mJointName = _joint.mJointName;
			joint.ResizeKeyframes(_joint.mKeyframes.length);
			_joint.mKeyframes.forEach((_frame, f) => {
				const kf = joint.GetKeyframe(f);
				kf.mTime = _frame.mTime;
				kf.mTranslation = [..._frame.mTranslation.value];
				kf.mRotation = [..._frame.mRotation.value];
			});
		});
		return anim;
	}
};
