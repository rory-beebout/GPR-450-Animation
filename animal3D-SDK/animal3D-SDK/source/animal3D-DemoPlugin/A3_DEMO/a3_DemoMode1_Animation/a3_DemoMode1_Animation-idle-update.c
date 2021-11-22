/*
	Copyright 2011-2020 Daniel S. Buckstein

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/

/*
	animal3D SDK: Minimal 3D Animation Framework
	By Daniel S. Buckstein
	
	a3_DemoMode1_Animation-idle-update.c
	Demo mode implementations: animation scene.

	********************************************
	*** UPDATE FOR ANIMATION SCENE MODE      ***
	********************************************
*/

//-----------------------------------------------------------------------------

#include "../a3_DemoMode1_Animation.h"

//typedef struct a3_DemoState a3_DemoState;
#include "../a3_DemoState.h"

#include "../_a3_demo_utilities/a3_DemoMacros.h"


//-----------------------------------------------------------------------------
// UTILS

inline a3real4r a3demo_mat2quat_safe(a3real4 q, a3real4x4 const m)
{
	// ****TO-DO: 
	//	-> convert rotation part of matrix to quaternion
	//	-> NOTE: this is for testing dual quaternion skinning only; 
	//		quaternion data would normally be computed with poses

	a3real4SetReal4(q, a3vec4_w.v);

	// done
	return q;
}

inline a3real4x2r a3demo_mat2dquat_safe(a3real4x2 Q, a3real4x4 const m)
{
	// ****TO-DO: 
	//	-> convert matrix to dual quaternion
	//	-> NOTE: this is for testing dual quaternion skinning only; 
	//		quaternion data would normally be computed with poses

	a3demo_mat2quat_safe(Q[0], m);
	a3real4SetReal4(Q[1], a3vec4_zero.v);

	// done
	return Q;
}


//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// UPDATE

void a3demo_update_objects(a3f64 const dt, a3_DemoSceneObject* sceneObjectBase,
	a3ui32 count, a3boolean useZYX, a3boolean applyScale);
void a3demo_update_defaultAnimation(a3_DemoState* demoState, a3f64 const dt,
	a3_DemoSceneObject* sceneObjectBase, a3ui32 count, a3ui32 axis);
void a3demo_update_bindSkybox(a3_DemoSceneObject* obj_camera, a3_DemoSceneObject* obj_skybox);
void a3demo_update_pointLight(a3_DemoSceneObject* obj_camera, a3_DemoPointLight* pointLightBase, a3ui32 count);

void a3demo_applyScale_internal(a3_DemoSceneObject* sceneObject, a3real4x4p s);

void a3animation_update_graphics(a3_DemoState* demoState, a3_DemoMode1_Animation* demoMode,
	a3_DemoModelMatrixStack const* matrixStack, a3_HierarchyState const* activeHS)
{
	// active camera
	a3_DemoProjector const* activeCamera = demoMode->projector + demoMode->activeCamera;
	a3_DemoSceneObject const* activeCameraObject = activeCamera->sceneObject;

	// temp scale mat
	a3mat4 scaleMat = a3mat4_identity;
	a3addressdiff const skeletonIndex = demoMode->obj_skeleton - demoMode->object_scene;
	a3ui32 const mvp_size = demoMode->hierarchy_skel->numNodes * sizeof(a3mat4);
	a3ui32 const t_skin_size = sizeof(demoMode->t_skin);
	a3ui32 const dq_skin_size = sizeof(demoMode->dq_skin);
	a3mat4 const mvp_obj = matrixStack[skeletonIndex].modelViewProjectionMat;
	a3mat4* mvp_joint, * mvp_bone, * t_skin;
	a3dualquat* dq_skin;
	a3index i;
	a3i32 p;

	// update joint and bone transforms
	for (i = 0; i < demoMode->hierarchy_skel->numNodes; ++i)
	{
		mvp_joint = demoMode->mvp_joint + i;
		mvp_bone = demoMode->mvp_bone + i;
		t_skin = demoMode->t_skin + i;
		dq_skin = demoMode->dq_skin + i;

		// joint transform
		a3real4x4SetScale(scaleMat.m, a3real_sixth);
		a3real4x4Concat(activeHS->objectSpace->pose[i].transformMat.m, scaleMat.m);
		a3real4x4Product(mvp_joint->m, mvp_obj.m, scaleMat.m);

		// bone transform
		p = demoMode->hierarchy_skel->nodes[i].parentIndex;
		if (p >= 0)
		{
			// position is parent joint's position
			scaleMat.v3 = activeHS->objectSpace->pose[p].transformMat.v3;

			// direction basis is from parent to current
			a3real3Diff(scaleMat.v2.v,
				activeHS->objectSpace->pose[i].transformMat.v3.v, scaleMat.v3.v);

			// right basis is cross of some upward vector and direction
			// select 'z' for up if either of the other dimensions is set
			a3real3MulS(a3real3CrossUnit(scaleMat.v0.v,
				a3real2LengthSquared(scaleMat.v2.v) > a3real_zero
				? a3vec3_z.v : a3vec3_y.v, scaleMat.v2.v), a3real_sixth);

			// up basis is cross of direction and right
			a3real3MulS(a3real3CrossUnit(scaleMat.v1.v,
				scaleMat.v2.v, scaleMat.v0.v), a3real_sixth);
		}
		else
		{
			// if we are a root joint, make bone invisible
			a3real4x4SetScale(scaleMat.m, a3real_zero);
		}
		a3real4x4Product(mvp_bone->m, mvp_obj.m, scaleMat.m);

		// get base to current object-space
		*t_skin = activeHS->objectSpaceBindToCurrent->pose[i].transformMat;

		// calculate DQ
		a3demo_mat2dquat_safe(dq_skin->Q, t_skin->m);
	}

	// upload
	a3bufferRefill(demoState->ubo_transformMVP, 0, mvp_size, demoMode->mvp_joint);
	a3bufferRefill(demoState->ubo_transformMVPB, 0, mvp_size, demoMode->mvp_bone);
	a3bufferRefill(demoState->ubo_transformBlend, 0, t_skin_size, demoMode->t_skin);
	a3bufferRefillOffset(demoState->ubo_transformBlend, 0, t_skin_size, dq_skin_size, demoMode->dq_skin);
}

void a3animation_update_fk(a3_HierarchyState* activeHS,
	a3_HierarchyState const* baseHS, a3_HierarchyPoseGroup const* poseGroup)
{
	if (activeHS->hierarchy == baseHS->hierarchy &&
		activeHS->hierarchy == poseGroup->hierarchy)
	{
		// FK pipeline
		a3hierarchyPoseConcat(activeHS->localSpace,	// local: goal to calculate
			activeHS->animPose, // holds current sample pose
			baseHS->localSpace, // holds base pose (animPose is all identity poses)
			activeHS->hierarchy->numNodes);
		a3hierarchyPoseConvert(activeHS->localSpace,
			activeHS->hierarchy->numNodes,
			poseGroup->channel,
			poseGroup->order);
		a3kinematicsSolveForward(activeHS);
	}
}

void a3animation_update_ik(a3_HierarchyState* activeHS,
	a3_HierarchyState const* baseHS, a3_HierarchyPoseGroup const* poseGroup)
{
	if (activeHS->hierarchy == baseHS->hierarchy &&
		activeHS->hierarchy == poseGroup->hierarchy)
	{
		// IK pipeline
		// ****TO-DO: direct opposite of FK

		a3kinematicsSolveInverse(activeHS);
		a3hierarchyPoseRestore(activeHS->localSpace,
			activeHS->hierarchy->numNodes,
			poseGroup->channel,
			poseGroup->order);
		a3hierarchyPoseDeconcat(activeHS->animPose,
			activeHS->localSpace,
			baseHS->localSpace,
			activeHS->hierarchy->numNodes);
	}
}

void a3animation_update_ik_single(a3_HierarchyState* activeHS,
	a3_HierarchyState const* baseHS, a3_HierarchyPoseGroup const* poseGroup, const a3ui32 index, const a3ui32 parentIndex)
{
	if (activeHS->hierarchy == baseHS->hierarchy &&
		activeHS->hierarchy == poseGroup->hierarchy)
	{
		a3kinematicsSolveInverseSingle(activeHS, index, parentIndex);
		a3spatialPoseRestore(activeHS->localSpace->pose + index, *poseGroup->channel, *poseGroup->order);
		a3spatialPoseDeconcat(activeHS->animPose->pose + index, activeHS->localSpace->pose + index, baseHS->localSpace->pose + index);
	}
}

void a3animation_update_ik_partial(a3_HierarchyState* activeHS,
	a3_HierarchyState const* baseHS, a3_HierarchyPoseGroup const* poseGroup, const a3ui32 index, const a3ui32 numNodes)
{
	if (activeHS->hierarchy == baseHS->hierarchy &&
		activeHS->hierarchy == poseGroup->hierarchy)
	{
		a3kinematicsSolveInversePartial(activeHS, index, numNodes);
		const a3_HierarchyNode* itr = activeHS->hierarchy->nodes + index;
		const a3_HierarchyNode* const end = itr + numNodes;
		for (; itr < end; ++itr)
		{
			a3spatialPoseRestore(activeHS->localSpace->pose + itr->index, *poseGroup->channel, *poseGroup->order);
			a3spatialPoseDeconcat(activeHS->animPose->pose + itr->index, activeHS->localSpace->pose + itr->index, baseHS->localSpace->pose + itr->index);
		}
	}
}

void a3animation_update_skin(a3_HierarchyState* activeHS,
	a3_HierarchyState const* baseHS)
{
	if (activeHS->hierarchy == baseHS->hierarchy)
	{
		// FK pipeline extended for skinning and other applications
		a3hierarchyStateUpdateLocalInverse(activeHS);
		a3hierarchyStateUpdateObjectInverse(activeHS);
		a3hierarchyStateUpdateObjectBindToCurrent(activeHS, baseHS);
	}
}

void a3animation_update_applyEffectors(a3_DemoMode1_Animation* demoMode,
	a3_HierarchyState* activeHS, a3_HierarchyState const* baseHS, a3_HierarchyPoseGroup const* poseGroup)
{
	if (activeHS->hierarchy == baseHS->hierarchy &&
		activeHS->hierarchy == poseGroup->hierarchy)
	{
		a3_DemoSceneObject* sceneObject = demoMode->obj_skeleton;
		a3ui32 j = sceneObject->sceneGraphIndex;

		// need to properly transform joints to their parent frame and vice-versa
		a3mat4 const controlToSkeleton = demoMode->sceneGraphState->objectSpaceInv->pose[j].transformMat;
		//a3mat4 const controlToSkeleton = demoMode->sceneGraphState->localSpaceInv->pose[j].transformMat;
		a3vec4 controlLocator_neckLookat, controlLocator_wristEffector, controlLocator_wristConstraint, controlLocator_wristBase;
		a3mat4 jointTransform_neck = a3mat4_identity, jointTransform_wrist = a3mat4_identity, jointTransform_elbow = a3mat4_identity, jointTransform_shoulder = a3mat4_identity;
		a3ui32 j_neck, j_wrist, j_elbow, j_shoulder;

		// NECK LOOK-AT
		{
			// look-at effector
			sceneObject = demoMode->obj_skeleton_neckLookat_ctrl;
			a3real4Real4x4Product(controlLocator_neckLookat.v, controlToSkeleton.m,
				demoMode->sceneGraphState->localSpace->pose[sceneObject->sceneGraphIndex].transformMat.v3.v);
			//controlLocator_neckLookat = demoMode->sceneGraphState->localSpace->pose[sceneObject->sceneGraphIndex].transformMat.v3;
			j = j_neck = a3hierarchyGetNodeIndex(activeHS->hierarchy, "mixamorig:Neck");
			jointTransform_neck = activeHS->objectSpace->pose[j].transformMat;

			// make "look-at" matrix
			// in this example, +Z is towards locator, +Y is up
			a3vec4 x,y,z;
			a3vec4 up = { 0,1,0,0 };

			a3real4Diff(z.v, controlLocator_neckLookat.v, jointTransform_neck.v3.v);
			a3real4Normalize(z.v);
			a3real3CrossUnit(x.v, up.xyz.v, z.xyz.v);
			a3real3CrossUnit(y.v, z.xyz.v, x.xyz.v);
			a3mat4 neckMat = { x.v0, x.v1, x.v2, 0,
							  y.v0, y.v1, y.v2, 0,
							  z.v0, z.v1, z.v2, 0,
							  jointTransform_neck.v3.x, jointTransform_neck.v3.y, jointTransform_neck.v3.z, 1 };

			// Built-in way >>> a3real4x4MakeLookAt seems to look away instead, so pass in transforms in opposite slots, and reset position manually.
			//a3real4x4MakeLookAt(targetTransformMat.m, 0, controlLocator_neckLookat.v, jointTransform_neck.v3.v, up.v);
			//targetTransformMat.v3 = jointTransform_neck.v3;

			// reassign resolved transforms to OBJECT-SPACE matrices
			// resolve local and animation pose for affected joint
			//	(instead of doing IK for whole skeleton when only one joint has changed)
			activeHS->objectSpace->pose[j_neck].transformMat = neckMat;
			a3real4x4TransformInvert(neckMat.m);
			activeHS->objectSpaceInv->pose[j_neck].transformMat = neckMat;

			a3animation_update_ik_single(activeHS, baseHS, poseGroup, j_neck, j_neck - 1);
		}

		// RIGHT ARM REACH
		{
			// right wrist effector
			sceneObject = demoMode->obj_skeleton_wristEffector_r_ctrl;
			a3real4Real4x4Product(controlLocator_wristEffector.v, controlToSkeleton.m,
				demoMode->sceneGraphState->localSpace->pose[sceneObject->sceneGraphIndex].transformMat.v3.v);
			j = j_wrist = a3hierarchyGetNodeIndex(activeHS->hierarchy, "mixamorig:RightHand");
			jointTransform_wrist = activeHS->objectSpace->pose[j].transformMat;

			// right wrist constraint
			sceneObject = demoMode->obj_skeleton_wristConstraint_r_ctrl;
			a3real4Real4x4Product(controlLocator_wristConstraint.v, controlToSkeleton.m,
				demoMode->sceneGraphState->localSpace->pose[sceneObject->sceneGraphIndex].transformMat.v3.v);
			j = j_elbow = a3hierarchyGetNodeIndex(activeHS->hierarchy, "mixamorig:RightForeArm");
			jointTransform_elbow = activeHS->objectSpace->pose[j].transformMat;

			// right wrist base
			j = j_shoulder = a3hierarchyGetNodeIndex(activeHS->hierarchy, "mixamorig:RightArm");
			jointTransform_shoulder = activeHS->objectSpace->pose[j].transformMat;
			controlLocator_wristBase = jointTransform_shoulder.v3;

			// solve positions and orientations for joints
			// in this example, +X points away from child, +Y is normal
			// 1) check if solution exists
			//	-> get vector between base and end effector; if it extends max length, straighten limb
			//	-> position of end effector's target is at the minimum possible distance along this vector
			a3real sideLength1, sideLength2, triangleArea, baseToEndLength, triangleHeight, s, triangleBaseLength;
			a3vec4 up = { 0,1,0,0 };
			a3vec3 baseToEnd, baseToConstraint, x, y, z;
			a3vec3 triangleUp, planeNormal;

			a3vec4 wristPos;
			a3mat4 elbowMat;
			a3mat4 shoulderMat;
			
			// get side lengths
			sideLength1 = a3real3Distance(baseHS->objectSpace->pose[j_shoulder].transformMat.v3.v, baseHS->objectSpace->pose[j_elbow].transformMat.v3.v);
			sideLength2 = a3real3Distance(baseHS->objectSpace->pose[j_elbow].transformMat.v3.v, baseHS->objectSpace->pose[j_wrist].transformMat.v3.v);

			// get shoulder-to-wrist vector
			a3real3Diff(baseToEnd.v, controlLocator_wristEffector.v, jointTransform_shoulder.v3.v);
			a3real3Normalize(baseToEnd.v);

			// get length of base-to-end
			baseToEndLength = a3real3Distance(controlLocator_wristEffector.v, jointTransform_shoulder.v3.v);

			// if effector can't be reached, straighten arm
			if (baseToEndLength > sideLength1 + sideLength2)
			{
				// get base-to-constraint vector
				a3real3Diff(baseToConstraint.v, controlLocator_wristConstraint.v, jointTransform_shoulder.v3.v);
				a3real3CrossUnit(planeNormal.v, baseToConstraint.v, baseToEnd.v);
				y = planeNormal;

				// set Shoulder matrix
				a3real3GetNegative(x.v, baseToEnd.v);
				a3real3CrossUnit(z.v, x.v, y.v);
				a3real4x4Set(shoulderMat.m, x.v0, x.v1, x.v2, 0,
											y.v0, y.v1, y.v2, 0,
											z.v0, z.v1, z.v2, 0,
									jointTransform_shoulder.v3.x, jointTransform_shoulder.v3.y, jointTransform_shoulder.v3.z, 1);

				// set elbow matrix
				a3vec3 elbowPos = { jointTransform_shoulder.v3.x + baseToEnd.x * sideLength1, jointTransform_shoulder.v3.y + baseToEnd.y * sideLength1, jointTransform_shoulder.v3.z + baseToEnd.z * sideLength1 };
				a3real4x4Set(elbowMat.m, x.v0, x.v1, x.v2, 0,
										 y.v0, y.v1, y.v2, 0,
										 z.v0, z.v1, z.v2, 0,
									elbowPos.x, elbowPos.y, elbowPos.z, 1);
				// set wrist position
				a3real4Set(wristPos.v, elbowPos.x + baseToEnd.x * sideLength1, elbowPos.y + baseToEnd.y * sideLength1, elbowPos.z + baseToEnd.z * sideLength1, 1);
			}
			else
			{
				// get shoulder-to-constraint vector
				a3real3Diff(baseToConstraint.v, controlLocator_wristConstraint.v, jointTransform_shoulder.v3.v);

				// calculate triangle plane and triangle "up" direction
				a3real3CrossUnit(planeNormal.v, baseToConstraint.v, baseToEnd.v);
				a3real3CrossUnit(triangleUp.v, baseToEnd.v, planeNormal.v);

				// Use Heron's formula to calculate triangle area, then solve for height, then base-length
				s = a3real_half * (baseToEndLength + sideLength1 + sideLength2);
				triangleArea = a3sqrtf(s * (s - baseToEndLength) * (s - sideLength1) * (s - sideLength2));
				triangleHeight = 2 * triangleArea / baseToEndLength;
				triangleBaseLength = a3sqrtf((sideLength1 * sideLength1) - (triangleHeight * triangleHeight));

				// calculate elbow position with our knowledge of the triangle
				// (BaseDirectionVector * BaseLength + TriangleUpVector * TriangleHeight + starting at the shoulder)
				a3vec3 elbowPos;
				a3real3MulS(baseToEnd.v, triangleBaseLength);
				a3real3MulS(triangleUp.v, triangleHeight);
				a3real3Sum(elbowPos.v, triangleUp.v, baseToEnd.v);
				a3real3Add(elbowPos.v, jointTransform_shoulder.v3.v);

				// set shoulder mat
				a3real3Diff(x.v, jointTransform_shoulder.v3.v, elbowPos.v);
				a3real3Normalize(x.v);
				y = planeNormal;
				a3real3CrossUnit(z.v, x.v, y.v);
				a3real4x4Set(shoulderMat.m, x.v0, x.v1, x.v2, 0,
											y.v0, y.v1, y.v2, 0,
											z.v0, z.v1, z.v2, 0,
								 jointTransform_shoulder.v3.x, jointTransform_shoulder.v3.y, jointTransform_shoulder.v3.z, 1);

				// set elbow mat
				a3real3Diff(x.v, elbowPos.v, controlLocator_wristEffector.v);
				a3real3Normalize(x.v);
				a3real3CrossUnit(z.v, x.v, y.v);
				a3real4x4Set(elbowMat.m, x.v0, x.v1, x.v2, 0,
										 y.v0, y.v1, y.v2, 0,
										 z.v0, z.v1, z.v2, 0,
								  elbowPos.x, elbowPos.y, elbowPos.z, 1 );

				// set wrist pos
				a3real4SetReal4(wristPos.v, controlLocator_wristEffector.v);
			}

			// reassign resolved transforms to OBJECT-SPACE matrices
			// work from root to leaf too get correct transformations
			activeHS->objectSpace->pose[j_shoulder].transformMat = shoulderMat;
			a3real4x4TransformInvert(shoulderMat.m);
			activeHS->objectSpaceInv->pose[j_shoulder].transformMat = shoulderMat;
			a3animation_update_ik_single(activeHS, baseHS, poseGroup, j_shoulder, j_shoulder - 1);

			activeHS->objectSpace->pose[j_elbow].transformMat = elbowMat;
			a3real4x4TransformInvert(elbowMat.m);
			activeHS->objectSpaceInv->pose[j_elbow].transformMat = elbowMat;
			a3animation_update_ik_single(activeHS, baseHS, poseGroup, j_elbow, j_shoulder);

			activeHS->objectSpace->pose[j_wrist].transformMat.v3 = wristPos;
			a3animation_update_ik_single(activeHS, baseHS, poseGroup, j_wrist, j_elbow);
		}
	}
}
void a3animation_sampleClipController(a3_DemoMode1_Animation* demoMode, a3f64 const dt,
	a3_ClipController* clipCtrl_fk, a3_HierarchyState* activeHS_fk,
	a3_HierarchyPoseGroup const* poseGroup)
{
	a3ui32 sampleIndex0, sampleIndex1;

	// resolve FK state
	// update clip controller, keyframe lerp
	a3clipControllerUpdate(clipCtrl_fk, dt);
	sampleIndex0 = demoMode->clipPool->keyframe[clipCtrl_fk->keyframeIndex].sampleIndex0;
	sampleIndex1 = demoMode->clipPool->keyframe[clipCtrl_fk->keyframeIndex].sampleIndex1;
	a3hierarchyPoseLerp(activeHS_fk->animPose,
		poseGroup->hpose + sampleIndex0, poseGroup->hpose + sampleIndex1,
		(a3real)clipCtrl_fk->keyframeParam, activeHS_fk->hierarchy->numNodes);
}
void a3animation_update_animation(a3_DemoMode1_Animation* demoMode, a3f64 const dt,
	a3boolean const updateIK)
{
	a3_HierarchyState* activeHS_fk = demoMode->hierarchyState_skel_fk;
	a3_HierarchyState* activeHS_ik = demoMode->hierarchyState_skel_ik;
	a3_HierarchyState* activeHS_walk = demoMode->hierarchyState_skel_walk;
	a3_HierarchyState* activeHS_run = demoMode->hierarchyState_skel_run;
	a3_HierarchyState* activeHS_jump = demoMode->hierarchyState_skel_jump;
	a3_HierarchyState* activeHS = demoMode->hierarchyState_skel_final;
	a3_HierarchyState const* baseHS = demoMode->hierarchyState_skel_base;
	a3_HierarchyPoseGroup const* poseGroup = demoMode->hierarchyPoseGroup_skel;

	// resolve FK state
	a3animation_sampleClipController(demoMode, dt, demoMode->clipCtrlA, activeHS_fk, poseGroup);
	// run FK pipeline
	a3animation_update_fk(activeHS_fk, baseHS, poseGroup);



	// blend FK/IK to final
	// testing: copy source to final
	//a3hierarchyPoseCopy(activeHS->animPose,	// dst: final anim
		//activeHS_fk->animPose,	// src: FK anim
	//	activeHS_ik->animPose,	// src: IK anim
		//baseHS->animPose,	// src: base anim (identity)
	//	activeHS->hierarchy->numNodes);

	// Create a blend node
	a3_HierarchyPoseBlendNode node;
	node.pose_out = activeHS->animPose;
	node.numNodes = demoMode->hierarchy_skel->numNodes;

	// to adjust playback-speed by character velocity
	a3f64 moveMagnitude = a3real2Length(demoMode->vel.v);
	
	// Sample poses from clip controllers
	a3f64 antiSkateWalk = 0.12f;
	a3f64 antiSkateRun = 0.06f;
	a3animation_sampleClipController(demoMode, dt, demoMode->clipCtrlA, activeHS_fk, poseGroup);
	a3animation_sampleClipController(demoMode, dt * moveMagnitude * antiSkateWalk, demoMode->clipCtrlB, activeHS_walk, poseGroup);
	a3animation_sampleClipController(demoMode, dt * moveMagnitude * antiSkateRun, demoMode->clipCtrlC, activeHS_run, poseGroup);
	a3animation_sampleClipController(demoMode, dt, demoMode->clipCtrlD, activeHS_jump, poseGroup);

	a3f64 walkParam = a3clamp(0, 1, moveMagnitude);
	a3f64 runParam = a3clamp(0, 1, moveMagnitude - 6);

	// Lerp from idle to walk animation
	node.op = a3hierarchyPoseOpLERP;
	node.param[0] = &walkParam;
	node.pose_ctrl[0] = activeHS_fk->animPose;
	node.pose_ctrl[1] = activeHS_walk->animPose; 
	a3hierarchyPoseBlendNodeCall(&node);

	// If moving fast enough, do running animation.
	if (moveMagnitude >= 6)
	{
		node.param[0] = &runParam;
		node.pose_ctrl[0] = activeHS_walk->animPose;
		node.pose_ctrl[1] = activeHS_run->animPose;
		a3hierarchyPoseBlendNodeCall(&node);
	}

	if (demoMode->doInverseKinematics)
	{
		// resolve IK state
		// copy FK to IK
		node.op = a3hierarchyPoseOpCopy;
		node.pose_out = activeHS_ik->animPose;
		node.pose_ctrl[0] = activeHS->animPose;
		a3hierarchyPoseBlendNodeCall(&node);

		// run FK
		a3animation_update_fk(activeHS_ik, baseHS, poseGroup);
		if (updateIK)
		{
			// invert object-space
			a3hierarchyStateUpdateObjectInverse(activeHS_ik);
			// run solvers
			a3animation_update_applyEffectors(demoMode, activeHS_ik, baseHS, poseGroup);
		}

		// Since our IK was solved from our final blendPose, just copy ik into our pose-to-display
		node.op = a3hierarchyPoseOpCopy;
		node.pose_out = activeHS->animPose;
		node.pose_ctrl[0] = activeHS_ik->animPose;
		a3hierarchyPoseBlendNodeCall(&node);
	}

	// run FK pipeline (skinning optional)
	a3animation_update_fk(activeHS, baseHS, poseGroup);
	a3animation_update_skin(activeHS, baseHS);

	// Root motion to negate movement away from origin;
	a3vec4 rootMotion = { activeHS->animPose->pose->translate.x, activeHS->animPose->pose->translate.z, activeHS->animPose->pose->translate.y, 0 };
	a3real3GetNegative(demoMode->obj_skeleton->position.v, rootMotion.xyz.v);
}

void a3animation_update_sceneGraph(a3_DemoMode1_Animation* demoMode, a3f64 const dt)
{
	a3ui32 i;
	a3mat4 scaleMat = a3mat4_identity;

	a3demo_update_objects(dt, demoMode->object_scene, animationMaxCount_sceneObject, 0, 0);
	a3demo_update_objects(dt, demoMode->obj_camera_main, 1, 1, 0);

	a3demo_updateProjectorViewProjectionMat(demoMode->proj_camera_main);

	// apply scales to objects
	for (i = 0; i < animationMaxCount_sceneObject; ++i)
	{
		a3demo_applyScale_internal(demoMode->object_scene + i, scaleMat.m);
	}

	// update skybox
	a3demo_update_bindSkybox(demoMode->obj_camera_main, demoMode->obj_skybox);

	for (i = 0; i < animationMaxCount_sceneObject; ++i)
		demoMode->sceneGraphState->localSpace->pose[i].transformMat = demoMode->object_scene[i].modelMat;
	a3kinematicsSolveForward(demoMode->sceneGraphState);
	a3hierarchyStateUpdateLocalInverse(demoMode->sceneGraphState);
	a3hierarchyStateUpdateObjectInverse(demoMode->sceneGraphState);
}

void a3animation_update(a3_DemoState* demoState, a3_DemoMode1_Animation* demoMode, a3f64 const dt)
{
	a3ui32 i;
	a3_DemoModelMatrixStack matrixStack[animationMaxCount_sceneObject];

	// active camera
	a3_DemoProjector const* activeCamera = demoMode->projector + demoMode->activeCamera;
	a3_DemoSceneObject const* activeCameraObject = activeCamera->sceneObject;

	// skeletal
	if (demoState->updateAnimation)
		a3animation_update_animation(demoMode, dt, 1);

	// update scene graph local transforms
	a3animation_update_sceneGraph(demoMode, dt);

	// update matrix stack data using scene graph
	for (i = 0; i < animationMaxCount_sceneObject; ++i)
	{
		a3demo_updateModelMatrixStack(matrixStack + i,
			activeCamera->projectionMat.m,
			demoMode->sceneGraphState->objectSpace->pose[demoMode->obj_camera_main->sceneGraphIndex].transformMat.m,
			demoMode->sceneGraphState->objectSpaceInv->pose[demoMode->obj_camera_main->sceneGraphIndex].transformMat.m,
			demoMode->sceneGraphState->objectSpace->pose[demoMode->object_scene[i].sceneGraphIndex].transformMat.m,
			a3mat4_identity.m);
	}

	// prepare and upload graphics data
	a3animation_update_graphics(demoState, demoMode, matrixStack, demoMode->hierarchyState_skel_final);

	// testing: reset IK effectors to lock them to FK result
	{
		//void a3animation_load_resetEffectors(a3_DemoMode1_Animation * demoMode,
		//	a3_HierarchyState * hierarchyState, a3_HierarchyPoseGroup const* poseGroup);
		//a3animation_load_resetEffectors(demoMode,
		//	demoMode->hierarchyState_skel_final, demoMode->hierarchyPoseGroup_skel);
	}

	// process input
	a3real distMult = 3;
	a3real speedMult = 5 + (4 * demoMode->sprint);
	switch (demoMode->ctrl_position)
	{
	case animation_input_direct:
		demoMode->pos.x = (a3real)demoMode->axis_l[0] * distMult;
		demoMode->pos.y = (a3real)demoMode->axis_l[1] * distMult;
		break;
	case animation_input_euler:
		demoMode->vel.x = (a3real)demoMode->axis_l[0] * speedMult;
		demoMode->vel.y = (a3real)demoMode->axis_l[1] * speedMult;
		demoMode->pos.x += demoMode->vel.x * (a3real)dt;
		demoMode->pos.y += demoMode->vel.y * (a3real)dt;
		break;
	case animation_input_kinematic:
		demoMode->acc.x = (a3real)demoMode->axis_l[0] * speedMult;
		demoMode->acc.y = (a3real)demoMode->axis_l[1] * speedMult;
		demoMode->vel.x += demoMode->acc.x * (a3real)dt;
		demoMode->vel.y += demoMode->acc.y * (a3real)dt;
		demoMode->pos.x += demoMode->vel.x * (a3real)dt;
		demoMode->pos.y += demoMode->vel.y * (a3real)dt;
		break;
	case animation_input_interpolate1:
		demoMode->pos.x = a3lerp(demoMode->pos.x, (a3real)demoMode->axis_l[0] * distMult, (a3real)(dt * speedMult));
		demoMode->pos.y = a3lerp(demoMode->pos.y, (a3real)demoMode->axis_l[1] * distMult, (a3real)(dt * speedMult));
		break;
	case animation_input_interpolate2:
		//demoMode->vel = a3vec2_zero; // set zero so it doesn't inherit velocity from other movement styles and fly off-screen.
		demoMode->vel.x = a3lerp(demoMode->vel.x, (a3real)demoMode->axis_l[0] * speedMult, (a3real)(dt * distMult));
		demoMode->vel.y = a3lerp(demoMode->vel.y, (a3real)demoMode->axis_l[1] * speedMult, (a3real)(dt * distMult));
		demoMode->pos.x += demoMode->vel.x * (a3real)dt;
		demoMode->pos.y += demoMode->vel.y * (a3real)dt;
		break;
	}

	a3real rotMultDeg = 180;
	// Integrate rotation
	switch (demoMode->ctrl_rotation)
	{
	case animation_input_direct:
		demoMode->rot = (a3real)demoMode->axis_r[0] * rotMultDeg;
		break;
	case animation_input_euler:
		demoMode->velr = (a3real)demoMode->axis_r[0] * rotMultDeg;
		demoMode->rot += demoMode->velr * (a3real)dt;
		break;
	case animation_input_kinematic:
		demoMode->accr = (a3real)demoMode->axis_r[0] * rotMultDeg;
		demoMode->velr += demoMode->accr * (a3real)dt;
		demoMode->rot += demoMode->velr * (a3real)dt;
		break;
	case animation_input_interpolate1:
		demoMode->rot = a3lerp(demoMode->rot, (a3real)demoMode->axis_r[0] * rotMultDeg, (a3real)dt);
		break;
	case animation_input_interpolate2:
		demoMode->velr = a3lerp(demoMode->velr, (a3real)demoMode->axis_r[0] * rotMultDeg, (a3real)(dt * speedMult));
		demoMode->rot += demoMode->velr * (a3real)dt;
		break;
	}

	// apply input
	demoMode->obj_skeleton_ctrl->position.x = +(demoMode->pos.x);
	demoMode->obj_skeleton_ctrl->position.y = +(demoMode->pos.y);
	demoMode->obj_skeleton_ctrl->euler.z = -a3trigValid_sind(demoMode->rot);
	
}


//-----------------------------------------------------------------------------
