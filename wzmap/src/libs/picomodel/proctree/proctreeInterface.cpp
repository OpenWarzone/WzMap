/*
	proctree c++ port test / example
	Copyright (c) 2015 Jari Komppa

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgement in the product documentation would be
	appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.

	NOTE: this license covers this example only, proctree.cpp has different license
*/

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include "proctree.h"
#include <string>

extern "C" {
	/* dependencies */
#include "picointernal.h"
}

extern std::string AssImp_getBasePath(const std::string& path);

void benchmark()
{
	Proctree::Tree tree;
	printf("Start..\n");
	int n = 100000;
	int j;
	for (j = 0; j < 10; j++)
	{
		std::chrono::time_point<std::chrono::system_clock> start, end;
		start = std::chrono::system_clock::now();
		int i;
		for (i = 0; i < n; i++)
		{
			tree.generate();
		}
		end = std::chrono::system_clock::now();
		std::chrono::duration<double> sec = end-start;
		printf("%3.3fs (%3.3f trees per second)\n", sec.count(), n / sec.count());
	}
}

// Generate a procedural tree model based on inputs...
picoModel_t *PicoGenerateTreeModel(char *name, char *barkShader, char *leafShader, void *treePropertyValues)
{
	// 1) Create the tree object

	Proctree::Tree tree;

	// 2) Change properties here for different kinds of trees
	/*
	tree.mProperties.mSeed = 7;
	tree.mProperties.mTreeSteps = 7;
	tree.mProperties.mLevels = 3;
	// etc.
	*/

	Proctree::Properties *treeProperties = (Proctree::Properties *)treePropertyValues;

	Proctree::Properties *mProperties = &tree.mProperties;

	mProperties->mSeed = (treeProperties && treeProperties->mSeed != 0) ? treeProperties->mSeed : 262;
	mProperties->mSegments = (treeProperties && treeProperties->mSegments != 0) ? treeProperties->mSegments : 6;
	mProperties->mLevels = (treeProperties && treeProperties->mLevels != 0) ? treeProperties->mLevels : 5;
	mProperties->mVMultiplier = (treeProperties && treeProperties->mVMultiplier != 0) ? treeProperties->mVMultiplier : 0.36f;
	mProperties->mTwigScale = (treeProperties && treeProperties->mTwigScale != 0) ? treeProperties->mTwigScale : 0.39f;
	mProperties->mInitialBranchLength = (treeProperties && treeProperties->mInitialBranchLength != 0) ? treeProperties->mInitialBranchLength : 0.49f;
	mProperties->mLengthFalloffFactor = (treeProperties && treeProperties->mLengthFalloffFactor != 0) ? treeProperties->mLengthFalloffFactor : 0.85f;
	mProperties->mLengthFalloffPower = (treeProperties && treeProperties->mLengthFalloffPower != 0) ? treeProperties->mLengthFalloffPower : 0.99f;
	mProperties->mClumpMax = (treeProperties && treeProperties->mClumpMax != 0) ? treeProperties->mClumpMax : 0.454f;
	mProperties->mClumpMin = (treeProperties && treeProperties->mClumpMin != 0) ? treeProperties->mClumpMin : 0.404f;
	mProperties->mBranchFactor = (treeProperties && treeProperties->mBranchFactor != 0) ? treeProperties->mBranchFactor : 2.45f;
	mProperties->mDropAmount = (treeProperties && treeProperties->mDropAmount != 0) ? treeProperties->mDropAmount : -0.1f;
	mProperties->mGrowAmount = (treeProperties && treeProperties->mGrowAmount != 0) ? treeProperties->mGrowAmount : 0.235f;
	mProperties->mSweepAmount = (treeProperties && treeProperties->mSweepAmount != 0) ? treeProperties->mSweepAmount : 0.01f;
	mProperties->mMaxRadius = (treeProperties && treeProperties->mMaxRadius != 0) ? treeProperties->mMaxRadius : 0.139f;
	mProperties->mClimbRate = (treeProperties && treeProperties->mClimbRate != 0) ? treeProperties->mClimbRate : 0.371f;
	mProperties->mTrunkKink = (treeProperties && treeProperties->mTrunkKink != 0) ? treeProperties->mTrunkKink : 0.093f;
	mProperties->mTreeSteps = (treeProperties && treeProperties->mTreeSteps != 0) ? treeProperties->mTreeSteps : 5;
	mProperties->mTaperRate = (treeProperties && treeProperties->mTaperRate != 0) ? treeProperties->mTaperRate : 0.947f;
	mProperties->mRadiusFalloffRate = (treeProperties && treeProperties->mRadiusFalloffRate != 0) ? treeProperties->mRadiusFalloffRate : 0.73f;
	mProperties->mTwistRate = (treeProperties && treeProperties->mTwistRate != 0) ? treeProperties->mTwistRate : 3.02f;
	mProperties->mTrunkLength = (treeProperties && treeProperties->mTrunkLength != 0) ? treeProperties->mTrunkLength : 2.4f;

	// 3) Call generate
	tree.generate();

	// 4) Use the data

	/* create new pico model */
	picoModel_t *model = PicoNewModel();

	if (model == NULL)
	{
		// No longer need the scene data...
		return NULL;
	}

	/* do model setup */
	PicoSetModelFrameNum(model, 0);
	PicoSetModelName(model, name);
	PicoSetModelFileName(model, name);

	std::string basePath = AssImp_getBasePath(name);

	int i;

	/* plain white */
	picoColor_t white = { 255,255,255,255 };

	/* create new pico surface */
	picoSurface_t *surface = PicoNewSurface(model);

	if (surface == NULL)
	{
		PicoFreeModel(model);
		return NULL;
	}

	/* do surface setup */
	PicoSetSurfaceType(surface, PICO_TRIANGLES);
	PicoSetSurfaceName(surface, barkShader);

	for (i = 0; i < tree.mVertCount; i++)
	{
		/*
		printf("%+3.3f, %+3.3f, %+3.3f,    %+3.3f, %+3.3f, %+3.3f,    %+3.3f, %+3.3f,\n",
			tree.mVert[i].x, tree.mVert[i].y, tree.mVert[i].z,
			tree.mNormal[i].x, tree.mNormal[i].y, tree.mNormal[i].z,
			tree.mUV[i].u, tree.mUV[i].v);
			*/

			/* store vertex origin */

		picoVec3_t vert;
		vert[0] = tree.mVert[i].x;
		vert[1] = tree.mVert[i].y;
		vert[2] = tree.mVert[i].z;
		PicoSetSurfaceXYZ(surface, i, vert);

		/* store vertex color */
		PicoSetSurfaceColor(surface, 0, i, white);

		/* store vertex normal */
		picoVec3_t norm;
		norm[0] = tree.mNormal[i].x;
		norm[1] = tree.mNormal[i].y;
		norm[2] = tree.mNormal[i].z;
		PicoSetSurfaceNormal(surface, i, norm);

		/* store current face vertex index */
		PicoSetSurfaceIndex(surface, i, (picoIndex_t)i);

		/* get texture vertex coord */
		picoVec2_t texCoord;
		texCoord[0] = tree.mUV[i].u;
		texCoord[1] = -tree.mUV[i].v;

		/* store texture vertex coord */
		PicoSetSurfaceST(surface, 0, i, texCoord);
	}

	/* create new pico surface */
	surface = PicoNewSurface(model);

	if (surface == NULL)
	{
		PicoFreeModel(model);
		return NULL;
	}

	/* do surface setup */
	PicoSetSurfaceType(surface, PICO_TRIANGLES);
	PicoSetSurfaceName(surface, leafShader);
	
	for (i = 0; i < tree.mTwigVertCount; i++)
	{
		/*printf("%+3.3f, %+3.3f, %+3.3f,    %+3.3f, %+3.3f, %+3.3f,    %+3.3f, %+3.3f,\n",
			tree.mTwigVert[i].x, tree.mTwigVert[i].y, tree.mTwigVert[i].z,
			tree.mTwigNormal[i].x, tree.mTwigNormal[i].y, tree.mTwigNormal[i].z,
			tree.mTwigUV[i].u, tree.mTwigUV[i].v);*/

		picoVec3_t vert;
		vert[0] = tree.mVert[i].x;
		vert[1] = tree.mVert[i].y;
		vert[2] = tree.mVert[i].z;
		PicoSetSurfaceXYZ(surface, i, vert);

		/* store vertex color */
		PicoSetSurfaceColor(surface, 0, i, white);

		/* store vertex normal */
		picoVec3_t norm;
		norm[0] = tree.mNormal[i].x;
		norm[1] = tree.mNormal[i].y;
		norm[2] = tree.mNormal[i].z;
		PicoSetSurfaceNormal(surface, i, norm);

		/* store current face vertex index */
		PicoSetSurfaceIndex(surface, i, (picoIndex_t)i);

		/* get texture vertex coord */
		picoVec2_t texCoord;
		texCoord[0] = tree.mUV[i].u;
		texCoord[1] = -tree.mUV[i].v;

		/* store texture vertex coord */
		PicoSetSurfaceST(surface, 0, i, texCoord);
	}

	// 5) Profit.

	// Note: You can change the properties and call generate to change the data, 
	// no need to delete the tree object in between.
}
