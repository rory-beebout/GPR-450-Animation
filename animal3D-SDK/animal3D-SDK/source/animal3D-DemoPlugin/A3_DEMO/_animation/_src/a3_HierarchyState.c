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
	
	modified by Rory Beebout, Jonathan Deleon

	a3_HierarchyState.c
	Implementation of transform hierarchy state.
*/

#include "../a3_HierarchyState.h"

#include <stdlib.h>
#include <string.h>


//-----------------------------------------------------------------------------

// initialize pose set given an initialized hierarchy and key pose count
a3i32 a3hierarchyPoseGroupCreate(a3_HierarchyPoseGroup *poseGroup_out, const a3_Hierarchy *hierarchy, const a3ui32 poseCount)
{
	// validate params and initialization states
	//	(output is not yet initialized, hierarchy is initialized)
	if (poseGroup_out && hierarchy && !poseGroup_out->hierarchy && hierarchy->nodes)
	{
		// determine memory requirements
		a3ui32 spatialPoseCount = poseCount * hierarchy->numNodes;
		a3ui32 spatialPoseSize = sizeof(a3_SpatialPose) * spatialPoseCount;
		a3ui32 hPosesSize = sizeof(a3_HierarchyPose) * poseCount;
		a3ui32 channelSize = sizeof(a3_SpatialPoseChannel) * hierarchy->numNodes;

		// allocate everything (one malloc)
		poseGroup_out->spatialPosePool = (a3_SpatialPose*)malloc(spatialPoseSize);
		poseGroup_out->HPoses = (a3_HierarchyPose*)malloc(hPosesSize);

		// set pointers
		poseGroup_out->hierarchy = hierarchy;
		poseGroup_out->poseCount = poseCount;
		poseGroup_out->spatialPoseCount = spatialPoseCount;
		poseGroup_out->channels = (a3_SpatialPoseChannel*)malloc(channelSize);
		
		a3ui32 i = 0;
		a3ui32 j = 0;
		for (i = 0; i < poseCount; i++)
		{
			j = hierarchy->numNodes * i;
			poseGroup_out->HPoses[i].spatialPose = poseGroup_out->spatialPosePool + j;
		}

		// reset all data
		for (i = 0; i < poseCount; i++)
		{
			a3hierarchyPoseReset(poseGroup_out->HPoses + i, hierarchy->numNodes);
		}

		memset(poseGroup_out->channels, a3poseChannel_none, channelSize);

		// done
		return 1;
	}
	return -1;
}

// release pose set
a3i32 a3hierarchyPoseGroupRelease(a3_HierarchyPoseGroup *poseGroup)
{
	// validate param exists and is initialized
	if (poseGroup && poseGroup->hierarchy)
	{
		// release everything (one free)
		//free(???);

		// reset pointers
		poseGroup->hierarchy = 0;

		// done
		return 1;
	}
	return -1;
}


//-----------------------------------------------------------------------------

// initialize hierarchy state given an initialized hierarchy
a3i32 a3hierarchyStateCreate(a3_HierarchyState *state_out, const a3_Hierarchy *hierarchy)
{
	// validate params and initialization states
	//	(output is not yet initialized, hierarchy is initialized)
	if (state_out && hierarchy && !state_out->hierarchy && hierarchy->nodes)
	{
		a3ui32 poseCount = 3; // 3 because 3 hposes (Sample, Object, and Local)

		// determine memory requirements
		a3ui32 hMemSize = sizeof(a3_HierarchyPose) * 3;
		a3ui32 poseMemSize = sizeof(a3_SpatialPose) * hierarchy->numNodes * poseCount; // * 3 because 3 hposes (Sample, Object, and Local)
		// allocate everything (one malloc)
		state_out->localSpacePose = (a3_HierarchyPose*)malloc(hMemSize + poseMemSize);
		
		// set pointers
		state_out->hierarchy = hierarchy;
		state_out->objectSpacePose = state_out->localSpacePose + 1;
		state_out->samplePose = state_out->localSpacePose + 2;

		state_out->localSpacePose->spatialPose = (a3_SpatialPose*)state_out->localSpacePose + hMemSize;
		state_out->objectSpacePose->spatialPose = state_out->localSpacePose->spatialPose + 1;
		state_out->samplePose->spatialPose = state_out->localSpacePose->spatialPose + 2;

		// reset all data
		a3hierarchyPoseReset(state_out->localSpacePose, hierarchy->numNodes);
		a3hierarchyPoseReset(state_out->objectSpacePose, hierarchy->numNodes);
		a3hierarchyPoseReset(state_out->samplePose, hierarchy->numNodes);

		// done
		return 1;
	}
	return -1;
}

// release hierarchy state
a3i32 a3hierarchyStateRelease(a3_HierarchyState *state)
{
	// validate param exists and is initialized
	if (state && state->hierarchy)
	{
		// release everything (one free)
		//free(???);

		// reset pointers
		state->hierarchy = 0;

		// done
		return 1;
	}
	return -1;
}


//-----------------------------------------------------------------------------

// load HTR file, read and store complete pose group and hierarchy
a3i32 a3hierarchyPoseGroupLoadHTR(a3_HierarchyPoseGroup* poseGroup_out, a3_Hierarchy* hierarchy_out, const a3byte* resourceFilePath)
{
	if (poseGroup_out && !poseGroup_out->poseCount && hierarchy_out && !hierarchy_out->numNodes && resourceFilePath && *resourceFilePath)
	{

	}
	return -1;
}

// load BVH file, read and store complete pose group and hierarchy
a3i32 a3hierarchyPoseGroupLoadBVH(a3_HierarchyPoseGroup* poseGroup_out, a3_Hierarchy* hierarchy_out, const a3byte* resourceFilePath)
{
	if (poseGroup_out && !poseGroup_out->poseCount && hierarchy_out && !hierarchy_out->numNodes && resourceFilePath && *resourceFilePath)
	{

	}
	return -1;
}

// save HTR file, write and store complete pose group and hierarchy
a3i32 a3hierarchyPoseGroupSaveHTR(a3_HierarchyPoseGroup* const poseGroup_in, a3_Hierarchy* const hierarchy_in, const a3byte* resourceFilePath)
{
	if (poseGroup_in && !poseGroup_in->poseCount && hierarchy_in && !hierarchy_in->numNodes && resourceFilePath && *resourceFilePath)
	{

	}
	return -1;
}
// save BVH file, write and store complete pose group and hierarchy
a3i32 a3hierarchyPoseGroupSaveBVH(a3_HierarchyPoseGroup* const poseGroup_in, a3_Hierarchy* const hierarchy_in, const a3byte* resourceFilePath)
{
	if (poseGroup_in && !poseGroup_in->poseCount && hierarchy_in && !hierarchy_in->numNodes && resourceFilePath && *resourceFilePath)
	{

	}
	return -1;
}

//-----------------------------------------------------------------------------
